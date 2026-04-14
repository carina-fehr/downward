#include "pdb_heuristic.h"
#include "pattern_information.h"

#include "pattern_database.h"

#include "../plugins/plugin.h"
#include "../utils/markup.h"
#include "../task_utils/task_properties.h"
#include "../task_utils/successor_generator.h"

#include <limits>
#include <memory>

using namespace std;

namespace pdbs {
static plugins::TypedEnumPlugin<PreferredOperatorsType> _enum_plugin(
    {{"none", "do not compute preferred operators"},
     {"precomputed", "compute preferred operators during PDB construction"},
     {"live", "compute preferred operators during heuristic evaluation"}});

static shared_ptr<PatternDatabase> get_pdb_from_generator(
    const shared_ptr<AbstractTask> &task,
    const shared_ptr<PatternGenerator> &pattern_generator, 
    PreferredOperatorsType use_preferred_operators, bool test_distances) {
    PatternInformation pattern_info = pattern_generator->generate(task);
    return pattern_info.get_pdb(use_preferred_operators, test_distances);
}

PDBHeuristic::PDBHeuristic(
    const shared_ptr<PatternGenerator> &pattern, bool test_distances,
    PreferredOperatorsType use_preferred_operators, 
    const shared_ptr<AbstractTask> &transform, bool cache_estimates,
    const string &description, utils::Verbosity verbosity)
    : Heuristic(transform, cache_estimates, description, verbosity),
      pdb(get_pdb_from_generator(task, pattern, use_preferred_operators, test_distances)), use_preferred_operators(use_preferred_operators) {
}

int PDBHeuristic::compute_heuristic(const State &ancestor_state) {
    State state = convert_ancestor_state(ancestor_state);
    int h = pdb->get_value(state.get_unpacked_values());
    if (h == numeric_limits<int>::max())
        return DEAD_END;
    
    if (use_preferred_operators == PreferredOperatorsType::LIVE) { // live computation
        successor_generator::SuccessorGenerator &successor_generator =
         successor_generator::g_successor_generators[task_proxy];

        vector<OperatorID> applicable_operators;
        successor_generator.generate_applicable_ops(state, applicable_operators);

        for (const OperatorID op_id : applicable_operators) {
            const OperatorProxy &op = task_proxy.get_operators()[op_id];
            State succ = state.get_unregistered_successor(op); // find successors
            int h_succ = pdb->get_value(succ.get_unpacked_values()); // find successors h value
            if (h == op.get_cost() + h_succ && h_succ != numeric_limits<int>::max()) { 
                set_preferred(op);
            }
        }
    }
    
    if ( use_preferred_operators == PreferredOperatorsType::PRECOMPUTED) { // pre-computation of preferred operators 
        const vector<OperatorID> &preferred_ops = pdb->get_preferred_operators(state.get_unpacked_values());

        for (OperatorID operator_no : preferred_ops) {
            OperatorProxy op = task_proxy.get_operators()[operator_no];
            assert(task_properties::is_applicable(op, state));
            set_preferred(op);
        } 
    }
    return h;
}

static basic_string<char> paper_references() {
    return utils::format_conference_reference(
               {"Stefan Edelkamp"}, "Planning with Pattern Databases",
               "https://aaai.org/papers/7280-ecp-01-2001/",
               "Proceedings of the Sixth European Conference on Planning (ECP 2001)",
               "84-90", "AAAI Press", "2001") +
           "For implementation notes, see:" +
           utils::format_conference_reference(
               {"Silvan Sievers", "Manuela Ortlieb", "Malte Helmert"},
               "Efficient Implementation of Pattern Database Heuristics for"
               " Classical Planning",
               "https://ai.dmi.unibas.ch/papers/sievers-et-al-socs2012.pdf",
               "Proceedings of the Fifth Annual Symposium on Combinatorial"
               " Search (SoCS 2012)",
               "105-111", "AAAI Press", "2012");
}
class PDBHeuristicFeature
    : public plugins::TypedFeature<Evaluator, PDBHeuristic> {
public:
    PDBHeuristicFeature() : TypedFeature("pdb") {
        document_subcategory("heuristics_pdb");
        document_title("Pattern database heuristic");
        document_synopsis(
            "Computes goal distance in "
            "state space abstractions based on projections. "
            "First used in domain-independent planning by:" +
            paper_references());

        add_option<shared_ptr<PatternGenerator>>(
            "pattern", "pattern generation method", "greedy()");
        add_option<bool>("test_distances", "run expensive tests to verify the distances are correct", "false");
        add_option<PreferredOperatorsType>("pref", "enable preferred operators, live or precomputed", "none");
        add_heuristic_options_to_feature(*this, "pdb");

        document_language_support("action costs", "supported");
        document_language_support("conditional effects", "not supported");
        document_language_support("axioms", "not supported");

        document_property("admissible", "yes");
        document_property("consistent", "yes");
        document_property("safe", "yes");
        document_property("preferred operators", "yes");
    }

    virtual shared_ptr<PDBHeuristic> create_component(
        const plugins::Options &opts) const override {
        return plugins::make_shared_from_arg_tuples<PDBHeuristic>(
            opts.get<shared_ptr<PatternGenerator>>("pattern"), 
            opts.get<bool>("test_distances"),
            opts.get<PreferredOperatorsType>("pref"),
            get_heuristic_arguments_from_options(opts));
    }
};

static plugins::FeaturePlugin<PDBHeuristicFeature> _plugin;
}
