#include <algorithm>
#include <forward_list>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

// Used to calculate displacements while visiting the neighbors of a cell.
const int dx[4] = {-1, 0, 1, 0};
const int dy[4] = {0, -1, 0, 1};

enum CellState {
    wall, available, visited, flooded
};

/*
 * The fast mode is used to quickly generate a solution; on special grids
 * it even performs better than the regular one. The regular mode is used
 * to handle general, larger grids.
 */
enum EngineMode {
    fast, regular
};

typedef pair<int,int> Cell;
typedef vector<vector<CellState>> GridConfiguration;
typedef vector<vector<int>> GridStatistics;

#include "lower-bound.cpp"

// We seed the generator with some fixed value to ensure reproducibility.
mt19937 rnd(15);

// Solves the problem using a greedy approach with randomization. The key for this
// approach is a flexible and customizable heuristic function (see 'candidate' below).
vector<Cell> solveChunk(const int n, const int m, GridConfiguration &grid, const EngineMode mode) {
    // Stores the number of walls surrounding each available (free) cell.
    GridStatistics walls(n + 2, vector<int>(m + 2));
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            for (int k = 0; k < 4; k++)
                walls[i][j] += grid[i + dy[k]][j + dx[k]] == wall;

    vector<Cell> result;
    // Stores the number of flooded neighbors for each free cell.
    GridStatistics floodedNeighbors(n + 2, vector<int>(m + 2));
    // Stores cells whose neighbors may flood them, so they are good starting points for expanding
    // the search space.
    forward_list<Cell> candidates;

    /*
     * This function simulates flooding of cells using the Flood Fill algorithm.
     * See https://www.geeksforgeeks.org/flood-fill-algorithm/
     *
     * The dryRun parameter enables simulations to happen on the grid without
     * changing it.
     */
    auto flood = [&] (const int row, const int col, const bool dryRun = true) {
        vector<Cell> backlog;
        int front = 0;
        int total = 0;

        if (dryRun and mode == fast) {
            // Just count how many direct neighbors will be flooded if we flood the given cell.
            for (int i = 0; i < 4; i++)
                if (grid[row + dy[i]][col + dx[i]] == available and floodedNeighbors[row + dy[i]][col + dx[i]] == 1)
                    total++;
            return total;
        }

        // Registers cells that becomes flooded if we flood the specified cell.
        auto addNeighbors = [&] (const Cell &cell) {
            for (int i = 0; i < 4; i++) {
                const int nextRow = cell.first + dy[i];
                const int nextCol = cell.second + dx[i];
                if ((nextRow != row or nextCol != col) and grid[nextRow][nextCol] == available) {
                    floodedNeighbors[nextRow][nextCol]++;
                    if (floodedNeighbors[nextRow][nextCol] == 2)
                        backlog.emplace_back(nextRow, nextCol);
                    else if (!dryRun and floodedNeighbors[nextRow][nextCol] == 1)
                        candidates.emplace_front(nextRow, nextCol);
                }
            }
        };

        backlog.emplace_back(row, col);
        if (!dryRun)
            result.emplace_back(row, col);

        while (front < backlog.size()) {
            const auto cell = backlog[front++];
            total++;
            addNeighbors(cell);
            if (!dryRun)
                grid[cell.first][cell.second] = flooded;
        }

        // In dry run mode we need to revert the changes made to the floodedNeighbors data structure.
        if (dryRun)
            for (const auto &cell: backlog)
                for (int i = 0; i < 4; i++) {
                    const int nextRow = cell.first + dy[i];
                    const int nextCol = cell.second + dx[i];
                    if ((nextRow != row or nextCol != col) and grid[nextRow][nextCol] == available)
                        --floodedNeighbors[nextRow][nextCol];
                }
        return total;
    };

    // Stage 1: flood all cells that are surrounded by at least 3 walls (including border walls).
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            if (grid[i][j] == available and walls[i][j] >= 3)
                flood(i, j, false);

    // Create a list of available cells. We know that we have done our job if all of them become flooded.
    vector<Cell> cells;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            if (grid[i][j] == available)
                cells.emplace_back(i, j);

    // Randomize the order of cells to simulate various isometric transformations of the grid.
    shuffle(cells.begin(), cells.end(), rnd);

    /* Sort the cells based upon a simplified heuristic (an advanced version is used in the
     * 'candidate' function below), since at this moment we have little information about
     * the cells in the grid. Whenever we need to pick a cell to process a new connected component
     * we will select the one with the highest score. This is achieved by taking elements
     * from the back of this list.
     *
     * We must use a stable sorting algorithm to ensure that we don't diminish the effect
     * of the previous shuffling.
     */
    stable_sort(cells.begin(), cells.end(), [&] (const Cell &a, const Cell &b) {
        return walls[a.first][a.second] < walls[b.first][b.second];
    });

    // Stage 2: successively seek for the best candidate to flood and track progress.
    typedef pair<int,Cell> ScoredCandidate;

    /*
     * Each candidate is prioritized based upon the following weighted features:
     *
     * 1. Number of cells that can be additionally flooded if we flood the candidate.
     * 2. Number of flooded neighbors (less is better).
     * 3. Number of walls surrounding the candidate. Border cells are overall better
     *    candidates.
     * 4. Random jitter that breaks ties and introduces additional variability.
     *
     * The heuristic depends on the engine mode. In fast mode we put more emphasis on
     * avoiding cells that have many flooded neighbors, while in regular mode we put
     * more emphasis on the number of cells that can be additionally flooded.
     */
    auto candidate = [&] (const int row, const int col) {
        if (mode == regular)
            return ScoredCandidate(1000 * flood(row, col)
                                   - 100 * floodedNeighbors[row][col]
                                   + 10 * walls[row][col]
                                   + rnd() % 10,
                    Cell(row, col));
        return ScoredCandidate(-100000 * floodedNeighbors[row][col]
                               + 1000 * flood(row, col)
                               + 100 * walls[row][col]
                               + rnd() % 100,
                    Cell(row, col));
    };

    while (!cells.empty())
        if (candidates.empty()) {
            // It is crucial to pick a good starting cell for a new connected component.
            // We do this by selecting a cell surrounded by a largest number of walls.
            // This is achieved by taking elements from the back of the previously sorted list.
            const auto cell = cells.back();
            cells.pop_back();
            if (grid[cell.first][cell.second] == available)
                flood(cell.first, cell.second, false);
        } else {
            ScoredCandidate bestCandidate = ScoredCandidate(INT_MIN, Cell(0, 0));
            for (auto prev = candidates.before_begin(), curr = candidates.begin(); curr != candidates.end();) {
                const int row = curr->first;
                const int col = curr->second;

                if (grid[row][col] == flooded) {
                    curr++;
                    candidates.erase_after(prev);
                } else {
                    for (int i = 0; i < 4; i++) {
                        const int nextRow = row + dy[i];
                        const int nextCol = col + dx[i];
                        if (grid[nextRow][nextCol] == available)
                            bestCandidate = max(bestCandidate, candidate(nextRow, nextCol));
                    }
                    prev = curr++;
                }
            }

            if (bestCandidate.first > INT_MIN) {
                const auto cell = bestCandidate.second;
                flood(cell.first, cell.second, false);
            }
        }
    return result;
}

// Performs chunking as necessary and aggregates partial results.
vector<Cell> solve(const int n, const int m, const GridConfiguration &grid, const EngineMode mode) {
    if (n <= 1000 and m <= 1000) {
        GridConfiguration gridCopy(grid);
        return solveChunk(n, m, gridCopy, mode);
    }

    // We are handling blocks with maximum size of chunkSize x chunkSize.
    const int chunkSize = 300;
    vector<Cell> result;
    for (int i = 1; i <= n; i += chunkSize)
        for (int j = 1; j <= m; j += chunkSize) {
            const int bottom = min(i + chunkSize - 1, n);
            const int right = min(j + chunkSize - 1, m);

            GridConfiguration gridChunk(chunkSize + 2, vector<CellState>(chunkSize + 2));
            for (int k = i; k <= bottom; k++)
                for (int l = j; l <= right; l++)
                    gridChunk[k - i + 1][l - j + 1] = grid[k][l];

            auto partialResult = solveChunk(chunkSize, chunkSize, gridChunk, mode);
            for (auto &cell: partialResult) {
                cell.first += i - 1;
                cell.second += j - 1;
            }
            result.insert(result.end(), partialResult.cbegin(), partialResult.cend());
        }
    return result;
}

int main(int argc, char *argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);

    const EngineMode mode = (argc > 1 and string(argv[1]) == "--fast") ? fast : regular;
    cout << "Engine mode is set to " << (mode == fast ? "fast" : "regular") << endl;

    int n, m;
    cin >> n >> m;

    // Stores the initial state of the grid. A grid is expanded with an artificial border
    // around all sides simulating outer walls.
    GridConfiguration inputGrid(n + 2, vector<CellState>(m + 2));
    for (int i = 1; i <= n; i++) {
        string row;
        cin >> row;
        for (int j = 1; j <= m; j++)
            inputGrid[i][j] = row[j - 1] == '.' ? available : wall;
    }

    auto printResult = [&] (vector<Cell> &result) {
        // This code was adapted from the official distribution associated with this problem specification.
        auto compress = [&m] (vector<Cell> &result) {
            sort(result.begin(), result.end());
            vector<int> compressed(result.size());
            for (int last = 0, i = 0; i < result.size(); i++) {
                compressed[i] = m * (result[i].first - 1) + result[i].second - last;
                last = m * (result[i].first - 1) + result[i].second;
            }
            return compressed;
        };

        const vector<int> compressed = compress(result);
        cout << compressed.size() << endl;
        for (int i = 0; i < compressed.size(); i++)
            cout << compressed[i] << " ";
        cout << endl << "<==================>" << endl;
    };

    const int lowerBound = findLowerBound(n, m, inputGrid);
    cout << "The estimated lower bound is " << lowerBound << endl;

    int minAmount = INT_MAX;
    while (minAmount > lowerBound) {
        vector<Cell> result = solve(n, m, inputGrid, mode);
        if (result.size() < minAmount) {
            minAmount = result.size();
            printResult(result);
        }
    }
    cout << "The optimal solution has been found." << endl;
    return 0;
}
