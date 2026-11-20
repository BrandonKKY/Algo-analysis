// This work was completed with the assistance of Claude Sonnet 5.5.

#include "seam_methods.h"

#include <algorithm>
#include <queue>
#include <vector>

// Graph cut seam finder (Assignment 2, item 4).
//
// IDEA
//   Turn the image into a flow network so that the cheapest way to separate
//   the left side (source S) from the right side (sink T) is exactly the
//   cheapest seam. Then: minimum cut = maximum flow (max-flow min-cut theorem).
//
// THE GRAPH (image is w wide, h tall; one node per pixel, plus S and T)
//   1. S -> (y, 0)              capacity INF
//   2. (y, x) -> (y, x+1)       capacity e[y][x]   cutting this edge = removing pixel (y, x)
//   3. (y, x+1) -> (y, x)       capacity INF       backward edge
//   4. (y, x) -> (y-1, x-1)     capacity INF       diagonal edges
//      (y, x) -> (y+1, x-1)     capacity INF
//   5. (y, w-1) -> T            capacity e[y][w-1] cutting this edge = removing pixel (y, w-1)
//
// WHY THE CUT IS A VALID SEAM
//   A cut puts every node on the source side or on the sink side, and pays
//   for every edge going from the source side to the sink side. The minimum
//   cut never pays for an INF edge, so:
//   - Backward edges (3): if (y, x+1) is on the source side, (y, x) must be
//     too. So in each row the source side is columns 0..k with no gaps, and
//     the row is cut exactly once, right after column k. Pixel (y, k) is the
//     seam pixel and the cut pays e[y][k] for it.
//   - Diagonal edges (4): if (y, x) is on the source side, so are
//     (y-1, x-1) and (y+1, x-1). So the seam column in neighbouring rows
//     differs by at most 1, i.e. the seam is connected.
//   Cut cost = sum of the energies of the seam pixels, so the minimum cut is
//   the minimum-energy seam, the same optimum dynamic programming finds.
//
// NOTE on edge 5: the brief gives this edge INF capacity. That keeps the last
//   column on the sink side of every cut, so its pixels could never be
//   removed and the seam could be worse than the DP seam. Giving it the
//   pixel's energy treats the last column like every other column: the cut
//   behind it then means "remove pixel (y, w-1)", and the minimum cut is the
//   same optimal seam that DP finds.
//
// HORIZONTAL SEAMS: transpose the image (swap rows and columns), find a
//   vertical seam in it, and read the result as a horizontal seam.
//
// MAX-FLOW ALGORITHM: Edmonds-Karp
//   Repeat: BFS from S to T using only edges that still have capacity left
//   (the "residual graph"). Push as much flow as the tightest edge on that
//   path allows. Stop when BFS can no longer reach T, or when stopAll() is
//   called (one seam can take minutes on a large image).
//   Time: O(V * E^2) worst case, with V = w*h nodes and E = about 4*w*h edges.

namespace {

const double INF = 1e18;  // "infinite" capacity: larger than any seam cost

struct Edge {
    int to;      // node this edge points to
    double cap;  // capacity still available (residual capacity)
};

struct Graph {
    std::vector<Edge> edges;            // edges[i] and edges[i ^ 1] are a pair
    std::vector<std::vector<int>> adj;  // adj[u] = indices of the edges leaving u

    explicit Graph(int nodeCount) : adj(nodeCount) {}

    // Adds edge u -> v, plus its reverse edge v -> u with capacity 0.
    // The reverse edge lets a later path "undo" flow sent along u -> v.
    // Because they are added together, the reverse of edge i is edge i ^ 1
    // (0 and 1 are a pair, 2 and 3 are a pair, ...).
    void addEdge(int u, int v, double cap) {
        adj[u].push_back(static_cast<int>(edges.size()));
        edges.push_back({v, cap});
        adj[v].push_back(static_cast<int>(edges.size()));
        edges.push_back({u, 0.0});
    }
};

// BFS from s over edges that still have capacity.
// reached[v]    = true if v can be reached from s.
// parentEdge[v] = the edge used to arrive at v (to rebuild the path later).
// Returns true as soon as t is reached.
bool bfs(const Graph& g, int s, int t, std::vector<char>& reached, std::vector<int>& parentEdge) {
    reached.assign(g.adj.size(), 0);
    parentEdge.assign(g.adj.size(), -1);
    std::queue<int> q;
    reached[s] = 1;
    q.push(s);
    while (!q.empty()) {
        const int u = q.front();
        q.pop();
        for (int i : g.adj[u]) {
            const int v = g.edges[i].to;
            if (g.edges[i].cap > 0 && !reached[v]) {
                reached[v] = 1;
                parentEdge[v] = i;
                if (v == t) return true;
                q.push(v);
            }
        }
    }
    return false;
}

// Edmonds-Karp. Returns the max flow value (= capacity of the minimum cut).
double maxFlow(Graph& g, int s, int t) {
    double flow = 0.0;
    std::vector<char> reached;
    std::vector<int> parentEdge;
    while (!stopRequested() && bfs(g, s, t, reached, parentEdge)) {
        // Walk the path backwards from t to s to find its tightest edge.
        // edges[i ^ 1].to is the node that edge i starts from.
        double bottleneck = INF;
        for (int v = t; v != s; v = g.edges[parentEdge[v] ^ 1].to)
            bottleneck = std::min(bottleneck, g.edges[parentEdge[v]].cap);

        // Push that much flow along the path.
        for (int v = t; v != s; v = g.edges[parentEdge[v] ^ 1].to) {
            g.edges[parentEdge[v]].cap -= bottleneck;      // less room going forward
            g.edges[parentEdge[v] ^ 1].cap += bottleneck;  // more room to undo
        }
        flow += bottleneck;
    }
    return flow;
}

}  // namespace

// Find ONE vertical seam of `img` and store the removed column of each row
// in seam[y] (seam.size() == img.h, adjacent rows differ by at most 1).
bool findSeamGraphcut(const Image& img, Seam& seam, std::string& error) {
    const int w = img.w, h = img.h;
    if (w < 2 || h < 1) {
        error = "image too small to carve";
        return false;
    }
    const Grid<double> e = sobelEnergy(img);

    const int source = w * h, sink = w * h + 1;
    auto node = [w](int y, int x) { return y * w + x; };

    // Step 1: build the graph.
    Graph g(w * h + 2);
    for (int y = 0; y < h; ++y) {
        g.addEdge(source, node(y, 0), INF);
        for (int x = 0; x < w; ++x) {
            const double energy = e(y, x);
            if (x + 1 < w) {
                g.addEdge(node(y, x), node(y, x + 1), energy);  // cost of removing (y, x)
                g.addEdge(node(y, x + 1), node(y, x), INF);     // backward edge
            } else {
                g.addEdge(node(y, x), sink, energy);            // cost of removing (y, w-1)
            }
            if (x > 0 && y > 0) g.addEdge(node(y, x), node(y - 1, x - 1), INF);      // diagonal up
            if (x > 0 && y + 1 < h) g.addEdge(node(y, x), node(y + 1, x - 1), INF);  // diagonal down
        }
    }

    // Step 2: run max-flow. The flow value equals the cost of the best seam.
    maxFlow(g, source, sink);
    if (stopRequested()) {
        error = "stopped";
        return false;
    }

    // Step 3: find the source side of the minimum cut = every node the source
    // can still reach in the residual graph.
    std::vector<char> onSourceSide;
    std::vector<int> unused;
    bfs(g, source, sink, onSourceSide, unused);

    // Step 4: in each row, the rightmost pixel on the source side is the seam.
    seam.assign(h, 0);
    for (int y = 0; y < h; ++y) {
        int x = 0;
        while (x + 1 < w && onSourceSide[node(y, x + 1)]) ++x;
        seam[y] = x;
    }
    return true;
}

// Swaps rows and columns: pixel (x, y) of the result is pixel (y, x) of `img`.
Image transposeImage(const Image& img) { return transpose(img); }

// Find ONE horizontal seam of `img`: seam[x] is the row removed from column x
// (seam.size() == img.w, adjacent columns differ by at most 1).
// A horizontal seam of the image is a vertical seam of the transposed image.
bool findHorizontalSeamGraphcut(const Image& img, Seam& seam, std::string& error) {
    return findSeamGraphcut(transposeImage(img), seam, error);
}
