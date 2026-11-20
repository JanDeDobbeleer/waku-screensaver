#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <tuple>

#include "animation/effect_internal.h"

namespace waku::screensaver {
namespace {

// The grid around the word has room for tunnels and the ghost house.
constexpr int kPadX = 6;
constexpr int kPadY = 1;
constexpr int kStubLength = 4;
constexpr double kHoldFraction = 0.15;
constexpr std::array<int, 4> kDx{1, 0, -1, 0};
constexpr std::array<int, 4> kDy{0, 1, 0, -1};
using Adjacency = std::array<uint32_t, 4>;

class MazeBuilder {
 public:
  explicit MazeBuilder(Art& art)
      : art_(art),
        maze_(art.chase),
        columns_(static_cast<int>(art.columns())),
        rows_(static_cast<int>(art.rows.size())),
        width_(columns_ + 2 * kPadX),
        height_(rows_ + 2 * kPadY),
        grid_(static_cast<size_t>(width_ * height_), kNoNode) {}

  void build() {
    maze_ = ChaseMaze{};
    maze_.cellNode.assign(static_cast<size_t>(columns_ * rows_), kNoNode);
    for (int row = 0; row < rows_; ++row) {
      for (int column = 0; column < columns_; ++column) {
        if (static_cast<size_t>(column) >= art_.rows[static_cast<size_t>(row)].size() ||
            art_.rows[static_cast<size_t>(row)][static_cast<size_t>(column)] != L'#')
          continue;
        maze_.cellNode[static_cast<size_t>(row * columns_ + column)] =
            addNode(column, row, kLetterNode);
      }
    }
    if (maze_.nodes.empty()) return;
    letterCount_ = maze_.nodes.size();
    connectComponents();
    addHouse();
    buildAdjacency();
    buildPacmanWalk();
    buildGhostWalk();
    buildCatchWalk();
    buildTimes();
    buildWalls();
  }

 private:
  uint32_t addNode(int x, int y, uint8_t kind) {
    const auto node = static_cast<uint32_t>(maze_.nodes.size());
    maze_.nodes.push_back({x, y, kind});
    grid_[cellIndex(x, y)] = node;
    return node;
  }

  size_t cellIndex(int x, int y) const {
    return static_cast<size_t>((y + kPadY) * width_ + (x + kPadX));
  }

  bool inBounds(int x, int y) const {
    return x + kPadX >= 0 && x + kPadX < width_ && y + kPadY >= 0 && y + kPadY < height_;
  }

  uint32_t neighbourAt(uint32_t node, size_t direction) const {
    const auto& at = maze_.nodes[node];
    const int x = at.x + kDx[direction];
    const int y = at.y + kDy[direction];
    return inBounds(x, y) ? grid_[cellIndex(x, y)] : kNoNode;
  }

  bool isLetter(uint32_t node) const { return maze_.nodes[node].kind == kLetterNode; }
  bool isHouse(uint32_t node) const { return maze_.nodes[node].kind == kHouseNode; }

  // Letters can touch only diagonally, or be separated by gaps. Carve the shortest tunnel from
  // the connected set to the nearest other letter until the whole word is one maze.
  void connectComponents() {
    const size_t cells = grid_.size();
    while (true) {
      std::vector<char> inSet(maze_.nodes.size(), 0);
      std::vector<uint32_t> stack{0};
      inSet[0] = 1;
      while (!stack.empty()) {
        const uint32_t node = stack.back();
        stack.pop_back();
        for (size_t d = 0; d < 4; ++d) {
          const uint32_t next = neighbourAt(node, d);
          if (next != kNoNode && !inSet[next]) {
            inSet[next] = 1;
            stack.push_back(next);
          }
        }
      }
      if (std::ranges::all_of(inSet, [](char value) { return value != 0; })) return;

      std::vector<size_t> previous(cells, cells);
      std::vector<char> seen(cells, 0);
      std::vector<size_t> queue;
      for (uint32_t node = 0; node < maze_.nodes.size(); ++node) {
        if (!inSet[node]) continue;
        const size_t cell = cellIndex(maze_.nodes[node].x, maze_.nodes[node].y);
        seen[cell] = 1;
        queue.push_back(cell);
      }
      size_t found = cells;
      for (size_t head = 0; head < queue.size() && found == cells; ++head) {
        const size_t cell = queue[head];
        const int x = static_cast<int>(cell % static_cast<size_t>(width_));
        const int y = static_cast<int>(cell / static_cast<size_t>(width_));
        for (size_t d = 0; d < 4 && found == cells; ++d) {
          const int nx = x + kDx[d];
          const int ny = y + kDy[d];
          if (nx < 0 || nx >= width_ || ny < 0 || ny >= height_) continue;
          const size_t next = static_cast<size_t>(ny * width_ + nx);
          if (seen[next]) continue;
          if (grid_[next] != kNoNode) {
            found = cell;
          } else {
            seen[next] = 1;
            previous[next] = cell;
            queue.push_back(next);
          }
        }
      }
      if (found == cells) return;
      for (size_t cell = found; cell != cells && grid_[cell] == kNoNode; cell = previous[cell]) {
        addNode(static_cast<int>(cell % static_cast<size_t>(width_)) - kPadX,
                static_cast<int>(cell / static_cast<size_t>(width_)) - kPadY, kTunnelNode);
      }
    }
  }

  // A dead-end stub right of the word. Pacman never enters it, so the ghost always has a safe spot.
  void addHouse() {
    uint32_t anchor = 0;
    for (uint32_t node = 0; node < letterCount_; ++node) {
      const auto& candidate = maze_.nodes[node];
      const auto& best = maze_.nodes[anchor];
      if (candidate.x > best.x || (candidate.x == best.x && candidate.y < best.y)) anchor = node;
    }
    const auto at = maze_.nodes[anchor];
    for (int step = 1; step <= kStubLength; ++step) {
      if (!inBounds(at.x + step, at.y) || grid_[cellIndex(at.x + step, at.y)] != kNoNode) break;
      houseEnd_ = addNode(at.x + step, at.y, kHouseNode);
    }
  }

  void buildAdjacency() {
    adjacency_.assign(maze_.nodes.size(), {kNoNode, kNoNode, kNoNode, kNoNode});
    for (uint32_t node = 0; node < maze_.nodes.size(); ++node)
      for (size_t d = 0; d < 4; ++d) adjacency_[node][d] = neighbourAt(node, d);
  }

  size_t directionBetween(uint32_t from, uint32_t to) const {
    const int dx = maze_.nodes[to].x - maze_.nodes[from].x;
    const int dy = maze_.nodes[to].y - maze_.nodes[from].y;
    return dx > 0 ? 0 : dy > 0 ? 1 : dx < 0 ? 2 : 3;
  }

  // Breadth-first path from `from` to the first node accepted by `accept`; the path excludes
  // `from`. Directions are tried starting with `firstDirection` so equal paths go straight on.
  template <typename Accept>
  std::vector<uint32_t> shortestPath(uint32_t from, size_t firstDirection, bool allowHouse,
                                     Accept accept) const {
    std::vector<uint32_t> previous(maze_.nodes.size(), kNoNode);
    std::vector<char> seen(maze_.nodes.size(), 0);
    std::vector<uint32_t> queue{from};
    seen[from] = 1;
    for (size_t head = 0; head < queue.size(); ++head) {
      const uint32_t node = queue[head];
      if (node != from && accept(node)) {
        std::vector<uint32_t> path;
        for (uint32_t step = node; step != from; step = previous[step]) path.push_back(step);
        std::ranges::reverse(path);
        return path;
      }
      for (size_t k = 0; k < 4; ++k) {
        const uint32_t next = adjacency_[node][(firstDirection + k) % 4];
        if (next == kNoNode || seen[next] || (!allowHouse && isHouse(next))) continue;
        seen[next] = 1;
        previous[next] = node;
        queue.push_back(next);
      }
    }
    return {};
  }

  // Greedy nearest-dot sweep. Every step is a single move to a neighbouring cell.
  void buildPacmanWalk() {
    uint32_t start = 0;
    for (uint32_t node = 0; node < letterCount_; ++node) {
      const auto& candidate = maze_.nodes[node];
      const auto& best = maze_.nodes[start];
      if (candidate.x < best.x || (candidate.x == best.x && candidate.y < best.y)) start = node;
    }
    std::vector<char> eaten(maze_.nodes.size(), 0);
    maze_.pacWalk = {start};
    eaten[start] = 1;
    size_t remaining = letterCount_ - 1;
    size_t direction = 0;
    while (remaining > 0) {
      const auto path = shortestPath(maze_.pacWalk.back(), direction, false,
                                     [&](uint32_t node) { return isLetter(node) && !eaten[node]; });
      if (path.empty()) break;
      for (const uint32_t node : path) {
        direction = directionBetween(maze_.pacWalk.back(), node);
        maze_.pacWalk.push_back(node);
        if (isLetter(node) && !eaten[node]) --remaining;
        eaten[node] = 1;
      }
    }
    maze_.feastSteps = maze_.pacWalk.size() - 1;
  }

  // The ghost picks one move or stay per Pacman step. A time-expanded table marks the (step, node)
  // states from which it can still avoid Pacman and every cell next to him until the feast ends,
  // so it chases on the graph without ever passing through him.
  void buildGhostWalk() {
    if (houseEnd_ == kNoNode) return;
    const size_t steps = maze_.feastSteps + 1;
    const size_t count = maze_.nodes.size();
    const auto& pac = maze_.pacWalk;
    const auto safe = [&](size_t step, uint32_t node) {
      const uint32_t at = pac[step];
      return node != at && std::ranges::find(adjacency_[at], node) == adjacency_[at].end();
    };
    std::vector<char> alive(steps * count, 0);
    for (size_t step = steps; step-- > 0;) {
      for (uint32_t node = 0; node < count; ++node) {
        bool ok = safe(step, node);
        if (ok && step + 1 < steps) {
          ok = alive[(step + 1) * count + node] != 0;
          for (size_t d = 0; d < 4 && !ok; ++d) {
            const uint32_t next = adjacency_[node][d];
            ok = next != kNoNode && alive[(step + 1) * count + next] != 0;
          }
        }
        alive[step * count + node] = ok ? 1 : 0;
      }
    }
    maze_.ghostWalk.assign(steps, houseEnd_);
    if (!alive[houseEnd_]) return;
    const auto hold = static_cast<size_t>(static_cast<double>(steps) * kHoldFraction);
    std::vector<int> distance(count);
    std::vector<uint32_t> queue;
    uint32_t ghost = houseEnd_;
    for (size_t step = 0; step + 1 < steps; ++step) {
      uint32_t best = ghost;
      if (step >= hold) {
        std::ranges::fill(distance, -1);
        queue.assign(1, pac[step + 1]);
        distance[pac[step + 1]] = 0;
        for (size_t head = 0; head < queue.size(); ++head)
          for (const uint32_t next : adjacency_[queue[head]])
            if (next != kNoNode && distance[next] < 0) {
              distance[next] = distance[queue[head]] + 1;
              queue.push_back(next);
            }
        int bestDistance =
            alive[(step + 1) * count + ghost] ? distance[ghost] : std::numeric_limits<int>::max();
        for (const uint32_t next : adjacency_[ghost]) {
          if (next == kNoNode || !alive[(step + 1) * count + next] || distance[next] < 0) continue;
          if (distance[next] < bestDistance) {
            bestDistance = distance[next];
            best = next;
          }
        }
      } else if (!alive[(step + 1) * count + ghost]) {
        for (const uint32_t next : adjacency_[ghost])
          if (next != kNoNode && alive[(step + 1) * count + next]) {
            best = next;
            break;
          }
      }
      ghost = best;
      maze_.ghostWalk[step + 1] = ghost;
    }
  }

  // After the power pellet Pacman runs down the ghost.
  void buildCatchWalk() {
    if (maze_.ghostWalk.empty()) return;
    const uint32_t target = maze_.ghostWalk.back();
    const auto path =
        shortestPath(maze_.pacWalk.back(), 0, true, [&](uint32_t node) { return node == target; });
    maze_.pacWalk.insert(maze_.pacWalk.end(), path.begin(), path.end());
  }

  void buildTimes() {
    maze_.pacTime.assign(maze_.pacWalk.size(), 0.0f);
    maze_.eatTime.assign(maze_.nodes.size(), std::numeric_limits<float>::infinity());
    maze_.eatTime[maze_.pacWalk[0]] = 0.0f;
    for (size_t step = 1; step < maze_.pacWalk.size(); ++step) {
      const auto& from = maze_.nodes[maze_.pacWalk[step - 1]];
      const auto& to = maze_.nodes[maze_.pacWalk[step]];
      maze_.pacTime[step] = maze_.pacTime[step - 1] + (from.y == to.y ? 1.0f : 1.75f);
      float& eaten = maze_.eatTime[maze_.pacWalk[step]];
      if (eaten == std::numeric_limits<float>::infinity()) eaten = maze_.pacTime[step];
    }
  }

  // Outline of the corridors. Edges shared by a letter and a tunnel are left open as doors.
  void buildWalls() {
    struct Edge {
      int orientation;  // 0 horizontal, 1 vertical
      bool dim;
      int fixed;
      int start;
      auto key() const { return std::tie(orientation, dim, fixed, start); }
    };
    std::vector<Edge> edges;
    for (uint32_t node = 0; node < maze_.nodes.size(); ++node) {
      const auto& at = maze_.nodes[node];
      const bool dim = at.kind != kLetterNode;
      for (size_t d = 0; d < 4; ++d) {
        if (adjacency_[node][d] != kNoNode) continue;
        switch (d) {
          case 0:
            edges.push_back({1, dim, at.x + 1, at.y});
            break;
          case 1:
            edges.push_back({0, dim, at.y + 1, at.x});
            break;
          case 2:
            edges.push_back({1, dim, at.x, at.y});
            break;
          default:
            edges.push_back({0, dim, at.y, at.x});
            break;
        }
      }
    }
    std::ranges::sort(edges, [](const Edge& a, const Edge& b) { return a.key() < b.key(); });
    for (size_t first = 0; first < edges.size();) {
      size_t last = first;
      while (last + 1 < edges.size() && edges[last + 1].orientation == edges[first].orientation &&
             edges[last + 1].dim == edges[first].dim &&
             edges[last + 1].fixed == edges[first].fixed &&
             edges[last + 1].start == edges[last].start + 1)
        ++last;
      const auto fixed = static_cast<float>(edges[first].fixed);
      const auto from = static_cast<float>(edges[first].start);
      const auto to = static_cast<float>(edges[last].start + 1);
      if (edges[first].orientation == 0)
        maze_.walls.push_back({from, fixed, to, fixed, edges[first].dim});
      else
        maze_.walls.push_back({fixed, from, fixed, to, edges[first].dim});
      first = last + 1;
    }
  }

  Art& art_;
  ChaseMaze& maze_;
  int columns_;
  int rows_;
  int width_;
  int height_;
  std::vector<uint32_t> grid_;
  std::vector<Adjacency> adjacency_;
  size_t letterCount_ = 0;
  uint32_t houseEnd_ = kNoNode;
};

}  // namespace

void buildChaseMaze(Art& art) { MazeBuilder(art).build(); }

}  // namespace waku::screensaver
