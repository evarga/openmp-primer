#include <iostream>
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
#include "engine.cpp"

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

    const bool runInParallel = shouldRunInParallel(n, m);
    cout << "Executing with max. " << (runInParallel ? omp_get_max_threads() : 1) << " thread(s)" << endl;

    int minAmount = INT_MAX;
    while (minAmount > lowerBound) {
        vector<Cell> result = solve(n, m, inputGrid, mode, runInParallel);
        if (result.size() < minAmount) {
            minAmount = result.size();
            printResult(result);
        }
    }
    cout << "The optimal solution has been found." << endl;
    return 0;
}
