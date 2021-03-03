#include "plan_manager.h"

#include "task_proxy.h"

#include "task_utils/task_properties.h"
#include "utils/logging.h"

#include <fstream>
#include <iostream>
#include <sstream>


using namespace std;

int calculate_plan_cost(const Plan &plan, const TaskProxy &task_proxy) {
    OperatorsProxy operators = task_proxy.get_operators();
    int plan_cost = 0;
    for (OperatorID op_id : plan) {
        plan_cost += operators[op_id].get_cost();
    }
    return plan_cost;
}

PlanManager::PlanManager()
    : plan_filename("sas_plan"),
      num_previously_generated_plans(0),
      is_part_of_anytime_portfolio(false) {
}

void PlanManager::set_plan_filename(const string &plan_filename_) {
    plan_filename = plan_filename_;
}

void PlanManager::set_failed_plans_filename(const string &failed_plans_filename_) {
    failed_plans_filename = failed_plans_filename_;
    if (failed_plans_filename != "")
        parse_failed_plans();
}

void PlanManager::set_num_previously_generated_plans(int num_previously_generated_plans_) {
    num_previously_generated_plans = num_previously_generated_plans_;
}

void PlanManager::set_is_part_of_anytime_portfolio(bool is_part_of_anytime_portfolio_) {
    is_part_of_anytime_portfolio = is_part_of_anytime_portfolio_;
}

void PlanManager::save_plan(
    const Plan &plan, const TaskProxy &task_proxy,
    bool generates_multiple_plan_files) {
    ostringstream filename;
    filename << plan_filename;
    int plan_number = num_previously_generated_plans + 1;
    if (generates_multiple_plan_files || is_part_of_anytime_portfolio) {
        filename << "." << plan_number;
    } else {
        assert(plan_number == 1);
    }
    ofstream outfile(filename.str());
    if (outfile.rdstate() & ofstream::failbit) {
        cerr << "Failed to open plan file: " << filename.str() << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    OperatorsProxy operators = task_proxy.get_operators();
    for (OperatorID op_id : plan) {
        cout << operators[op_id].get_name() << " (" << operators[op_id].get_cost() << ")" << endl;
        outfile << "(" << operators[op_id].get_name() << ")" << endl;
    }
    int plan_cost = calculate_plan_cost(plan, task_proxy);
    bool is_unit_cost = task_properties::is_unit_cost(task_proxy);
    outfile << "; cost = " << plan_cost << " ("
            << (is_unit_cost ? "unit cost" : "general cost") << ")" << endl;
    outfile.close();
    utils::g_log << "Plan length: " << plan.size() << " step(s)." << endl;
    utils::g_log << "Plan cost: " << plan_cost << endl;
    ++num_previously_generated_plans;
}

void PlanManager::parse_failed_plans() {
    ostringstream filename;
    string line;
    filename << failed_plans_filename;
    ifstream infile(filename.str());
    if (infile.rdstate() & ofstream::failbit) {
        cerr << "Failed to open failed plans file: " << filename.str() << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    vector<string> trace;
    bool found_failing_stage = false;
    bool end_of_failed_plan = false;
    while (getline(infile, line)){
        if (line == "=====") {
            // Separator between different failed plans
            failed_plans.push_back(trace);
            trace.clear();
            found_failing_stage = false;
            end_of_failed_plan = false;
        }
        else if (line.find(";") == string::npos) {
            // Line is not a comment
            if (line.find("(") == 0 && line.find(")") == line.size()-1) {
                line = line.substr(1, line.size()-2);
                if (line.substr(line.size()-3, line.size()-1) == "%%%") {
                    line = line.substr(0, line.size()-3);
                    found_failing_stage = true;
                }
            }
            else {
               cerr << "Failed to parse failed plan file, invalid format: missing '(' or ')' character" << endl;
               utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
            }
            if (!end_of_failed_plan) {
                trace.push_back(line);
                if (found_failing_stage)
                    end_of_failed_plan = true;
            }
        } else if (line.find(";") != 0) {
            cerr << "Failed to parse failed plan file, invalid use of comments: character ';' not the first in line " << endl;
            utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
        }
    }
    failed_plans.push_back(trace);
}
