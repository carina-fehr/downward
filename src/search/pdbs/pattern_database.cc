#include "pattern_database.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"
#include "../utils/math.h"
#include "../utils/rng.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

using namespace std;

namespace pdbs {
Projection::Projection(const TaskProxy &task_proxy, const Pattern &pattern)
    : pattern(pattern) {
    task_properties::verify_no_axioms(task_proxy);
    task_properties::verify_no_conditional_effects(task_proxy);
    assert(utils::is_sorted_unique(pattern));

    domain_sizes.reserve(pattern.size());
    hash_multipliers.reserve(pattern.size());
    num_abstract_states = 1;
    for (int pattern_var_id : pattern) {
        hash_multipliers.push_back(num_abstract_states);
        VariableProxy var = task_proxy.get_variables()[pattern_var_id];
        int domain_size = var.get_domain_size();
        domain_sizes.push_back(domain_size);
        if (utils::is_product_within_limit(
                num_abstract_states, domain_size, numeric_limits<int>::max())) {
            num_abstract_states *= domain_size;
        } else {
            cerr << "Given pattern is too large! (Overflow occured): " << endl;
            cerr << pattern << endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }
}

int Projection::rank(const vector<int> &state) const {
    size_t index = 0;
    for (size_t i = 0; i < pattern.size(); ++i) {
        index += hash_multipliers[i] * state[pattern[i]];
    }
    return index;
}

int Projection::unrank(int index, int var) const {
    int temp = index / hash_multipliers[var];
    return temp % domain_sizes[var];
}

PatternDatabase::PatternDatabase(
    Projection &&projection, vector<int> &&distances, vector<vector<OperatorID>> &&preferred_operators, vector<int> &&applicable_op_counts)
    : projection(move(projection)), distances(move(distances)), preferred_operators(move(preferred_operators)), applicable_op_counts(move(applicable_op_counts)) {
}

int PatternDatabase::get_value(const vector<int> &state) const {
    return distances[projection.rank(state)];
}

double PatternDatabase::compute_mean_finite_h() const {
    double sum = 0;
    int size = 0;
    for (size_t i = 0; i < distances.size(); ++i) {
        if (distances[i] != numeric_limits<int>::max()) {
            sum += distances[i];
            ++size;
        }
    }
    if (size == 0) { // All states are dead ends.
        return numeric_limits<double>::infinity();
    } else {
        return sum / size;
    }
}

void PatternDatabase::distance_test(const State &start_state, const TaskProxy &task) const {
    State state = start_state;
    int h_start = get_value(start_state.get_unpacked_values());
    int cost_sum = 0;
    int h = h_start;
    utils::RandomNumberGenerator rng;

    while (h > 0) { // not reached goal state yet
        int id = projection.rank(state.get_unpacked_values()); // get id of abstract state
        const vector<OperatorID> &ops = preferred_operators[id];

        if (ops.empty()) { // No preferred operator available for state
            return;
        }

        OperatorID random_op_id = *rng.choose(ops);
        const OperatorProxy &op = task.get_operators()[random_op_id]; 
        State succ = state.get_unregistered_successor(op); 
        int h_succ = get_value(succ.get_unpacked_values()); 
        cost_sum += op.get_cost(); 
        state = succ; 
        h = h_succ;
    }

    //cout << "sum of cost: " << cost_sum << "; start h value: " << h_start << endl;
    if (h_start != cost_sum) {
        cout << "heuristic value from inital state not equal to sum of cost" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}
}
