////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
// weights .hpp .cpp - Layer weights class
////////////////////////////////////////////////////////////////////////////////

#include "lbann/weights/weights.hpp"
#include "lbann/optimizers/optimizer.hpp"

namespace lbann {

weights::weights(lbann_comm* comm,
                 cudnn::cudnn_manager* cudnn)
  : m_comm(comm),
    m_cudnn(cudnn),
    m_values(nullptr),
    m_initializer(nullptr),
    m_optimizer(nullptr) {

  // Initialize weights name
  static int num_weights = 0;
  m_name = "weights" + std::to_string(num_weights);
  num_weights++;

  // Zero initialization is default
  if (m_initializer == nullptr) {
    m_initializer = new constant_initializer(m_comm, DataType(0));
  }

}

weights::weights(const weights& other) 
  : m_name(other.m_name),
    m_comm(other.m_comm),
    m_cudnn(other.m_cudnn),
    m_values(other.m_values),
    m_initializer(other.m_initializer),
    m_optimizer(other.m_optimizer) {

  // Create deep copy of pointers
  if (m_values != nullptr)      { m_values = m_values->Copy(); }
  if (m_initializer != nullptr) { m_initializer = m_initializer->copy(); }
  if (m_optimizer != nullptr) {
    m_optimizer = m_optimizer->copy();
    m_optimizer->set_weights(*this);
  }

}

weights& weights::operator=(const weights& other) {
  m_name = other.m_name;
  m_comm = other.m_comm;
  m_cudnn = other.m_cudnn;

  // Copy weights matrix
  if (m_values != nullptr && other.m_values != nullptr
      && m_values->DistData() == other.m_values->DistData()) {
    El::Copy(*other.m_values, *m_values);
  }
  if (m_values != nullptr) {
    delete m_values;
    m_values = nullptr;
  }
  if (other.m_values != nullptr) {
    m_values = other.m_values->Copy();
  }

  // Copy initializer
  if (m_initializer != nullptr) {
    delete m_initializer;
    m_initializer = nullptr;
  }
  if (other.m_initializer != nullptr) {
    m_initializer = other.m_initializer->copy();
  }

  // Copy optimizer
  if (m_optimizer != nullptr) {
    delete m_optimizer;
    m_optimizer = nullptr;
  }
  if (other.m_optimizer != nullptr) {
    m_optimizer = other.m_optimizer->copy();
    m_optimizer->set_weights(*this);
  }

  return *this;
}

weights::~weights() {
  if (m_values != nullptr)      { delete m_values; }
  if (m_initializer != nullptr) { delete m_initializer; }
  if (m_optimizer != nullptr)   { delete m_optimizer; }
}

void weights::setup(int height,
                    int width,
                    El::Distribution col_dist,
                    El::Distribution row_dist) {

  // Check if weights has already been set up
  if (m_values != nullptr) {
    const El::DistData dist_data(*m_values);
    if (m_values->Height() != height
        || m_values->Width() != width
        || dist_data.colDist != col_dist
        || dist_data.rowDist != row_dist) {
      std::stringstream err;
      err << __FILE__ << " " << __LINE__ << " :: "
          << "attempted to setup " << m_name << " with "
          << "height=" << height << ","
          << "width=" << width << ","
          << "col_dist=" << col_dist << ","
          << "row_dist=" << row_dist << ", "
          << "but the it is already setup with "
          << "height=" << m_values->Height() << ","
          << "width=" << m_values->Width() << ","
          << "col_dist=" << dist_data.colDist << ","
          << "row_dist=" << dist_data.rowDist;
      throw lbann_exception(err.str());
    } else {
      return;
    }
  }
  
  // Initialize weights matrix
  if (m_values != nullptr) { delete m_values; }
  m_values = m_initializer->construct_matrix(height, width, col_dist, row_dist);

  // Setup GPU objects
  if (m_cudnn != nullptr) {
    setup_gpu();
  }

}

void weights::setup_gpu() {
  #ifndef __LIB_CUDNN
  std::stringstream err;
  err << __FILE__ << " " << __LINE__ << " :: " << "cuDNN not detected";
  throw lbann_exception(err.str());
  #else
  #endif // __LIB_CUDNN
}

void weights::set_initializer(weights_initializer* initializer) {
  if (m_initializer != nullptr) { delete m_initializer; }
  m_initializer = initializer;
}


void weights::set_optimizer(optimizer* opt) {
  if (m_optimizer != nullptr) { delete m_optimizer; }
  m_optimizer = opt;
}

AbsDistMat& weights::get_values() {
  if (m_values == nullptr) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "attempted to access values of weights before they are setup";
    throw lbann_exception(err.str());
  }
  return *m_values;
}

const AbsDistMat& weights::get_values() const {
  if (m_values == nullptr) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "attempted to access values of weights before they are setup";
    throw lbann_exception(err.str());
  }
  return *m_values;
}

void weights::set_values(const AbsDistMat& values) {
  if (m_values == nullptr) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "attempted to set values of weights before they are setup";
    throw lbann_exception(err.str());
  }
  El::Copy(values, *m_values);
}

void weights::get_values_view(AbsDistMat& values_v) const {
  if (m_values == nullptr) {
    std::stringstream err;
    err << __FILE__ << " " << __LINE__ << " :: "
        << "attempted to access values of weights before they are setup";
    throw lbann_exception(err.str());
  }
  if (m_values->DistData() == values_v.DistData()) {
    El::LockedView(values_v, *m_values);
  }
  else {
    El::Copy(*m_values, values_v);
  }
}

}  // namespace lbann
