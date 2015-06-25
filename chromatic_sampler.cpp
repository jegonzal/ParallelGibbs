
#include "util.hpp"
#include "chromatic_sampler.hpp"
#include "run_statistics.hpp"
#include "global_variables.hpp"

// Include the macro for the foreach operation
#include <graphlab/macros_def.hpp>



void single_gibbs_update(mrf_gl::iscope& scope, 
                         mrf_gl::icallback& scheduler) {
  mrf_vertex_data& vdata = scope.vertex_data();
  //TODO: switch to use tls
  factor_t belief(vdata.variable);
  belief.uniform();
  foreach(const factor_id_t factor_id, vdata.factor_ids) {
    //const factor_t& factor(SHARED_FACTORS.get()[factor_id]);
    const factor_t& factor((*SHARED_FACTORS_PTR)[factor_id]);
    // build the conditional
    assignment_t conditional_asg = factor.args() - vdata.variable;
    for(size_t i = 0; i < conditional_asg.num_vars(); ++i) {
      const mrf_vertex_data& other_vdata = 
	scope.const_neighbor_vertex_data(conditional_asg.args().var(i).id());
      assert(conditional_asg.args().var(i) == other_vdata.variable);
      conditional_asg.set_asg_at(i, other_vdata.asg);
    }
    belief.times_condition(factor, conditional_asg);
  }
  belief.normalize();
  size_t new_asg = belief.sample().asg_at(0);
  vdata.nchanges += (new_asg != vdata.asg);
  vdata.asg = new_asg;
  vdata.belief += belief;
  vdata.nsamples++;  
}



// bool nsamples_terminator(const mrf_gl::ishared_data* shared_data) {
//   const size_t nsamples = n_samples.get_val();
//   bool terminate = nsamples >= MAX_NSAMPLES.get();
//   if(terminate) {
//     //     std::cout << "Termination condition reached" << std::endl;
//   }
//   return terminate;
// }


void run_chromatic_sampler(mrf_gl::core& core, 
                           const std::string& chromatic_results_fn,
                           const std::vector<double>& runtimes,
                           const bool draw_images) {
  // Initialize scheduler
  core.set_scheduler_type("chromatic");
  core.set_scope_type("null");

  size_t ncpus = core.engine().get_ncpus();

  // Disable sched yield to prevent thread sleeping at the end of a
  // color
  core.engine().set_sched_yield(false);
  // Use fixed update function
  core.sched_options().add_option("update_function", single_gibbs_update);
  
  double total_runtime = 0;
  double actual_total_runtime = 0;
  foreach(const double experiment_runtime, runtimes) {
    total_runtime += experiment_runtime;
    // get the experiment id
    size_t experiment_id = file_line_count(chromatic_results_fn);
    std::cout << "Running chromatic sampler experiment " << experiment_id
              << " for " << experiment_runtime << " seconds." << std::endl;
    // set the termination time
    core.engine().set_timeout(experiment_runtime);
    // Run the engine
    graphlab::timer timer;
    timer.start();
    core.start();
    double actual_experiment_runtime = timer.current_time();
    actual_total_runtime += actual_experiment_runtime;
    /// ==================================================================
    // Compute final statistics of the mode
    run_statistics stats(core.graph());
    stats.print();
    // Save the beliefs
    save_beliefs(core.graph(),
                 make_filename("chromatic_blfs_", ".tsv", experiment_id));
    // // Save the current assignments
    save_asg(core.graph(),
             make_filename("chromatic_asg_", ".asg", experiment_id));
    // Save the experiment
    std::ofstream fout(chromatic_results_fn.c_str(), std::ios::app);
    fout.precision(16);
    fout << experiment_id << '\t'
         << total_runtime << '\t'
         << actual_total_runtime << '\t'
         << ncpus << '\t'
         << stats.nsamples << '\t'
         << stats.nchanges << '\t'
         << stats.loglik << '\t'
         << stats.min_samples << '\t'
         << stats.max_samples << std::endl;
    fout.close();
    // Plot images if desired
    if(draw_images) draw_mrf(experiment_id, "chromatic", core.graph());
  }
} // end run_chromatic sampler
