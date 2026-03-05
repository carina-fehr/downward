#include "pattern_information.h"

#include "pattern_database.h"
#include "pattern_database_factory.h"
#include "validation.h"

#include <cassert>

using namespace std;

namespace pdbs {
PatternInformation::PatternInformation(
    const TaskProxy &task_proxy, Pattern pattern, utils::LogProxy &log)
    : task_proxy(task_proxy), pattern(move(pattern)), pdb(nullptr) {
    validate_and_normalize_pattern(task_proxy, this->pattern, log);
}

bool PatternInformation::information_is_valid() const {
    return !pdb || pdb->get_pattern() == pattern;
}

void PatternInformation::create_pdb_if_missing(int use_preferred_operators) {
    if (!pdb) {
        pdb = compute_pdb(task_proxy, pattern, vector<int>(), nullptr, use_preferred_operators);
    }
}

void PatternInformation::set_pdb(const shared_ptr<PatternDatabase> &pdb_) {
    pdb = pdb_;
    assert(information_is_valid());
}

const Pattern &PatternInformation::get_pattern() const {
    return pattern;
}

shared_ptr<PatternDatabase> PatternInformation::get_pdb(int use_preferred_operators) {
    create_pdb_if_missing(use_preferred_operators);
    return pdb;
}
}
