// Copyright 2018 The Simons Foundation, Inc. - All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SOURCES_SAMPLER_METROPOLIS_LOCAL_V2_HPP
#define SOURCES_SAMPLER_METROPOLIS_LOCAL_V2_HPP

#include <functional>
#include <limits>
#include <vector>

#include <Eigen/Core>
#include <nonstd/span.hpp>

#include "Machine/rbm_spin_v2.hpp"
#include "Operator/abstract_operator.hpp"
#include "Utils/exceptions.hpp"
#include "Utils/random_utils.hpp"

namespace netket {

struct Suggestion {
  nonstd::span<Index const> sites;
  nonstd::span<double const> values;
};

namespace detail {
/// Suggests which spins to try flipping next.
class Flipper {
 public:
  template <class T>
  using Vector = Eigen::Matrix<T, Eigen::Dynamic, 1>;

  inline Flipper(std::pair<Index, Index> const shape,
                 std::vector<double> local_states);

  Index BatchSize() const noexcept { return state_.rows(); }
  Index SystemSize() const noexcept { return state_.cols(); }
  default_random_engine& Generator() noexcept { return engine_.Get(); }

  /// \brief Resets the flipper.
  ///
  /// Randomizes internal state.
  inline void Reset();

  /// \brief Makes a move.
  ///
  /// \param accept has length #BatchSize() and describes which flips were
  /// accepted and which weren't (`accept[i]==true` means that the `i`th flip
  /// was accepted).
  inline void Next(nonstd::span<const bool> accept);

  /// \brief Returns the current state.
  ///
  /// Each row of the matrix describes one visible configuration. There are
  /// #BatchSize() rows in the returned matrix.
  const RowMatrix<double>& Current() const noexcept;

  /// \brief Returns next spins to try flipping.
  inline nonstd::span<Suggestion const> Read() const noexcept;

  /// \brief Similar to #Read() except that the result is written into \p x.
  inline void Read(Eigen::Ref<RowMatrix<double>> x) const noexcept;

 private:
  /// \brief Randomizes the state.
  ///
  /// New state is sampled uniformly from #local_states_
  inline void RandomState();

  /// \brief Randomizes the indices.
  ///
  /// New indices are sampled uniformly.
  inline void RandomSites();

  /// \brief Randomizes the values.
  ///
  /// New values are chosen uniformly from all possible quantum numbers except
  /// the current one (to minimize staying in the same configuration).
  inline void RandomValues();

  /// \brief A vector of size `BatchSize()` which indices of sites at which we
  /// suggest changing quantum numbers
  Vector<Index> sites_;
  /// \brief A vector of size `BatchSize()` which contains new (proposed)
  /// quantum numbers.
  Vector<double> values_;
  /// \brief A matrix of size `BatchSize() x SystemSize()` with the
  /// current state of #BatchSize() different Markov chains.
  RowMatrix<double> state_;
  /// \brief Allowed values for quantum numbers
  std::vector<double> local_states_;

  std::vector<Suggestion> proposed_;
  DistributedRandomEngine engine_;
};
}  // namespace detail

class MetropolisLocalV2 {
  RbmSpinV2& machine_;
  detail::Flipper flipper_;
  RowMatrix<double> proposed_X_;
  Eigen::ArrayXcd proposed_Y_;
  Eigen::ArrayXcd current_Y_;
  Eigen::ArrayXd randoms_;
  Eigen::Array<bool, Eigen::Dynamic, 1> accept_;

  inline MetropolisLocalV2(RbmSpinV2& machine, Index batch_size,
                           std::true_type);

 public:
  MetropolisLocalV2(RbmSpinV2& machine, Index batch_size);

  Index BatchSize() const noexcept { return flipper_.BatchSize(); }
  Index SystemSize() const noexcept { return flipper_.SystemSize(); }
  RbmSpinV2& Machine() const noexcept { return machine_; }

  /// Returns a batch of current visible states and corresponding log values.
  std::pair<Eigen::Ref<const RowMatrix<double>>,
            Eigen::Ref<const Eigen::VectorXcd>>
  Read();

  /// Makes a step.
  void Next();

  /// Resets the sampler.
  void Reset();
};

struct StepsRange {
  StepsRange(std::tuple<Index, Index, Index> const& steps)
      : start_{std::get<0>(steps)},
        end_{std::get<1>(steps)},
        step_{std::get<2>(steps)} {
    CheckValid();
  }

  Index start() const noexcept { return start_; }
  Index end() const noexcept { return end_; }
  Index step() const noexcept { return step_; }
  Index size() const noexcept { return (end_ - start_ - 1) / step_ + 1; }

 private:
  void CheckValid() const;

  Index start_;
  Index end_;
  Index step_;
};  // namespace netket

std::tuple<RowMatrix<double>, Eigen::VectorXcd,
           nonstd::optional<RowMatrix<Complex>>>
ComputeSamples(MetropolisLocalV2& sampler, StepsRange const& steps,
               bool compute_gradients);

Eigen::VectorXcd LocalValuesV2(Eigen::Ref<const RowMatrix<double>> samples,
                               Eigen::Ref<const Eigen::VectorXcd> values,
                               RbmSpinV2& machine, AbstractOperator& op,
                               Index batch_size);

Eigen::VectorXcd Gradient(Eigen::Ref<const Eigen::VectorXcd> values,
                          Eigen::Ref<const RowMatrix<Complex>> gradients);

}  // namespace netket

#endif  // SOURCES_SAMPLER_METROPOLIS_LOCAL_V2_HPP
