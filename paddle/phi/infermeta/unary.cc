/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/phi/infermeta/unary.h"

#include <algorithm>
#include <set>

#include "paddle/fluid/framework/convert_utils.h"
#include "paddle/phi/common/data_type.h"
#include "paddle/phi/common/type_traits.h"
#include "paddle/phi/core/enforce.h"
#include "paddle/phi/core/infermeta_utils.h"
#include "paddle/phi/kernels/funcs/unfold_functor.h"

namespace phi {

void ArgMinMaxInferMeta(const MetaTensor& x,
                        int64_t axis,
                        bool keepdims,
                        bool flatten,
                        int dtype,
                        MetaTensor* out,
                        MetaConfig config) {
  const auto& x_dims = x.dims();

  PADDLE_ENFORCE_GE(
      axis,
      -x_dims.size(),
      phi::errors::InvalidArgument("'axis'(%d) must be greater than or equal to"
                                   " -Rank(X)(%d).",
                                   axis,
                                   -x_dims.size()));
  PADDLE_ENFORCE_LT(axis,
                    x_dims.size(),
                    phi::errors::InvalidArgument(
                        "'axis'(%d) must be less than Rank(X)(%d) of Input(X).",
                        axis,
                        x_dims.size()));

  PADDLE_ENFORCE_EQ(
      (dtype < 0 || dtype == 2 || dtype == 3),
      true,
      phi::errors::InvalidArgument(
          "The attribute of dtype in argmin/argmax must be [%s] or [%s], but "
          "received [%s]",
          paddle::framework::DataTypeToString(
              paddle::framework::proto::VarType::INT32),
          paddle::framework::DataTypeToString(
              paddle::framework::proto::VarType::INT64),
          paddle::framework::DataTypeToString(
              static_cast<paddle::framework::proto::VarType::Type>(dtype))));

  auto x_rank = x_dims.size();
  if (axis < 0) axis += x_rank;
  if (config.is_runtime) {
    if (dtype == paddle::framework::proto::VarType::INT32) {
      int64_t all_element_num = 0;
      if (flatten) {
        all_element_num = phi::product(x_dims);

      } else {
        all_element_num = x_dims[axis];
      }
      PADDLE_ENFORCE_LE(
          all_element_num,
          INT_MAX,
          phi::errors::InvalidArgument(
              "The element num of the argmin/argmax input at axis is "
              "%d, is larger than int32 maximum value:%d, you must "
              "set the dtype of argmin/argmax to 'int64'.",
              all_element_num,
              INT_MAX));
    }
  }
  std::vector<int64_t> vec;
  if (flatten) {
    vec.emplace_back(static_cast<int64_t>(1));
  } else {
    for (int64_t i = 0; i < axis; i++) vec.emplace_back(x_dims[i]);
    if (keepdims) {
      vec.emplace_back(static_cast<int64_t>(1));
    }
    for (int64_t i = axis + 1; i < x_rank; i++) vec.emplace_back(x_dims[i]);
  }
  out->set_dims(phi::make_ddim(vec));
  if (dtype == 2) {
    out->set_dtype(DataType::INT32);
  } else if (dtype == 3) {
    out->set_dtype(DataType::INT64);
  }
}

void ArgsortInferMeta(const MetaTensor& input,
                      int axis,
                      bool descending,
                      MetaTensor* output,
                      MetaTensor* indices) {
  auto in_dims = input.dims();
  auto num_dims = in_dims.size();
  PADDLE_ENFORCE_GE(
      axis,
      -num_dims,
      phi::errors::InvalidArgument("'axis'(%d) must be greater than or equal to"
                                   " -num_dims(%d).",
                                   axis,
                                   -num_dims));
  PADDLE_ENFORCE_LT(
      axis,
      num_dims,
      phi::errors::InvalidArgument(
          "'axis'(%d) must be less than num_dims(%d).", axis, num_dims));

  output->share_dims(input);
  output->set_dtype(input.dtype());
  indices->share_dims(input);
  indices->set_dtype(DataType::INT64);
  output->share_lod(input);
  indices->share_lod(input);
}

void CastInferMeta(const MetaTensor& x, DataType out_dtype, MetaTensor* out) {
  out->set_dims(x.dims());
  out->set_dtype(out_dtype);
  out->set_layout(x.layout());
}

void CholeskyInferMeta(const MetaTensor& x, bool upper, MetaTensor* out) {
  auto dims = x.dims();
  auto rank = dims.size();
  PADDLE_ENFORCE_GE(rank,
                    2,
                    errors::InvalidArgument(
                        "The Input(X) should have at least 2 dimensions. But "
                        "received a %d dimension tensor.",
                        rank));
  PADDLE_ENFORCE_EQ(
      dims[rank - 2],
      dims[rank - 1],
      errors::InvalidArgument(
          "The inner-most 2 dimensions of Input(X) all should be symmetric "
          "positive-definite matrices and have the same size. But received "
          "X's shape[-2] = %d and shape[-1] = %d.",
          dims[rank - 2],
          dims[rank - 1]));
  out->set_dims(x.dims());
  out->set_dtype(x.dtype());
}

void CopyToInferMeta(const MetaTensor& x,
                     Backend backend,
                     bool blocking,
                     MetaTensor* out) {
  UnchangedInferMeta(x, out);
}

void CreateLikeInferMeta(const MetaTensor& x, DataType dtype, MetaTensor* out) {
  out->set_dims(x.dims());
  out->set_dtype(dtype == DataType::UNDEFINED ? x.dtype() : dtype);
  out->set_layout(x.layout());
}

void CumsumInferMeta(const MetaTensor& x,
                     int axis,
                     bool flatten,
                     bool exclusive,
                     bool reverse,
                     MetaTensor* out) {
  auto x_dims = x.dims();
  if (flatten) {
    out->set_dims(phi::make_ddim({phi::product(x_dims)}));
    out->set_dtype(x.dtype());
  } else {
    out->set_dims(x_dims);
    out->set_dtype(x.dtype());
  }

  out->share_lod(x);
}

void DiagInferMeta(const MetaTensor& x,
                   int offset,
                   float padding_value,
                   MetaTensor* out) {
  auto x_dims = x.dims();

  if (x_dims.size() == 1UL) {
    int64_t size_ = x_dims[0] + std::abs(offset);
    out->set_dims({size_, size_});
    out->set_dtype(x.dtype());
  } else if (x_dims.size() == 2UL) {
    int64_t size_ = 0;
    if (offset >= 0) {
      // Note(LutaoChu): Do not use std::min here, otherwise the calculation
      // of `size_` will have unexpected result on Windows Python3.8
      if (x_dims[0] < x_dims[1] - offset) {
        size_ = x_dims[0];
      } else {
        size_ = x_dims[1] - offset;
      }
    } else {
      // Note(LutaoChu): Do not use std::min here, otherwise the calculation
      // of `size_` will have unexpected result on Windows Python3.8
      if (x_dims[0] + offset < x_dims[1]) {
        size_ = x_dims[0] + offset;
      } else {
        size_ = x_dims[1];
      }
    }
    out->set_dims({size_});
    out->set_dtype(x.dtype());
  } else {
    PADDLE_THROW(phi::errors::InvalidArgument(
        "The input tensor X's dimensions of DiagV2Op should be either 1 or "
        "2, but received %d.",
        x_dims.size()));
  }
}

void DiagonalInferMeta(const MetaTensor& input,
                       int offset,
                       int axis1,
                       int axis2,
                       MetaTensor* out) {
  auto x_dims = input.dims();
  int offset_ = offset;
  int axis1_ = axis1 < 0 ? x_dims.size() + axis1 : axis1;
  int axis2_ = axis2 < 0 ? x_dims.size() + axis2 : axis2;

  PADDLE_ENFORCE_GE(
      x_dims.size(),
      2,
      phi::errors::OutOfRange("Input's dim is out of range (expected at "
                              "least 2 dimensions, but got %ld).",
                              x_dims.size()));
  PADDLE_ENFORCE_LT(
      axis1_,
      x_dims.size(),
      phi::errors::OutOfRange(
          "Attr(axis1) is out of range (expected to be in range of [%ld, "
          "%ld], but got %ld).",
          -(x_dims.size()),
          (x_dims.size() - 1),
          axis1));
  PADDLE_ENFORCE_LT(
      axis2_,
      x_dims.size(),
      phi::errors::OutOfRange(
          "Attr(axis2) is out of range (expected to be in range of [%ld, "
          "%ld], but got %ld).",
          -(x_dims.size()),
          (x_dims.size() - 1),
          axis2));
  PADDLE_ENFORCE_NE(
      axis1_,
      axis2_,
      phi::errors::InvalidArgument("The dimensions should not be identical "
                                   "%d vs %d.",
                                   axis1,
                                   axis2));

  auto out_dims = vectorize(x_dims);
  // from out_dims get the dim size of axis1_.
  auto axis1_size = out_dims[axis1_];
  auto axis2_size = out_dims[axis2_];
  // delete two dims by attr axis1 and axis2 from out_dims.
  /* example:
     out_dim = [2, 3, 4];
     axis1 = 0;
     axis2 = 1;
     according to the attr of axis1 and axis2, we get:
     out_dim = [4].
  */
  out_dims.erase(out_dims.begin() + std::max(axis1_, axis2_));
  out_dims.erase(out_dims.begin() + std::min(axis1_, axis2_));

  if (offset_ == 0) {
    out_dims.push_back(std::min(axis1_size, axis2_size));
  } else if (offset_ > 0) {
    if ((axis2_size - offset_) > 0) {
      out_dims.push_back(std::min(axis1_size, axis2_size - offset_));
    } else {
      out_dims.push_back(0);
    }
  } else {
    if ((axis1_size + offset_) > 0) {
      out_dims.push_back(std::min(axis1_size + offset_, axis2_size));
    } else {
      out_dims.push_back(0);
    }
  }
  out->set_dims(phi::make_ddim(out_dims));
}

void EighInferMeta(const MetaTensor& x,
                   const std::string& uplo,
                   MetaTensor* out_w,
                   MetaTensor* out_v) {
  auto input_dim = x.dims();
  auto rank = input_dim.size();

  PADDLE_ENFORCE_GE(rank,
                    2,
                    phi::errors::InvalidArgument(
                        "The Input(X) should have at least 2 dimensions."
                        "But received a %d dimension tensor.",
                        rank));
  PADDLE_ENFORCE_EQ(
      input_dim[rank - 2],
      input_dim[rank - 1],
      phi::errors::InvalidArgument(
          "Eigh op is designed for square matrix, consequently"
          "inner-most 2 dimensions of Input(X) should be symmetric."
          "But received X's shape[-2] = %d and shape[-1] = %d.",
          input_dim[rank - 2],
          input_dim[rank - 1]));

  std::vector<int64_t> values_dim;

  for (auto i = 0; i < rank - 1; i++) {
    values_dim.emplace_back(input_dim[i]);
  }
  out_w->set_dims(phi::make_ddim(values_dim));
  out_v->set_dims(input_dim);
}

void FlattenInferMeta(const MetaTensor& x,
                      int start_axis,
                      int stop_axis,
                      MetaTensor* out) {
  auto x_dims = x.dims();
  int in_dims_size = x_dims.size();
  if (start_axis < 0) {
    start_axis = start_axis + in_dims_size;
  }
  if (stop_axis < 0) {
    stop_axis = stop_axis + in_dims_size;
  }
  PADDLE_ENFORCE_GE(
      stop_axis,
      start_axis,
      phi::errors::InvalidArgument("The stop_axis should be greater"
                                   "than or equal to start_axis."));

  int64_t outer = 1;
  std::vector<int32_t> out_shape;
  out_shape.reserve(in_dims_size - stop_axis + start_axis);

  for (int i = 0; i < start_axis; ++i) {
    out_shape.push_back(x_dims[i]);
  }
  for (int i = start_axis; i <= stop_axis; i++) {
    if (x_dims[i] == -1 || outer == -1) {
      outer = -1;
    } else {
      outer *= x_dims[i];
    }
  }
  out_shape.push_back(outer);
  for (int i = stop_axis + 1; i < in_dims_size; i++) {
    out_shape.push_back(x_dims[i]);
  }
  const auto& out_dims = phi::make_ddim(out_shape);
  out->set_dims(out_dims);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());

  if (x_dims[0] == out_dims[0]) {
    // Only pass LoD when the first dimension of output and Input(X)
    // are the same.
    out->share_lod(x);
  }
}

void GumbelSoftmaxInferMeta(const MetaTensor& x,
                            float temperature,
                            bool hard,
                            int axis,
                            MetaTensor* out) {
  UnchangedInferMetaCheckAxis(x, axis, out);
}

void IncrementInferMeta(const MetaTensor& x, float value, MetaTensor* out) {
  PADDLE_ENFORCE_EQ(
      product(x.dims()),
      1UL,
      errors::InvalidArgument("The number of elements in Input(X) should be 1."
                              "Now the number is %d.",
                              product(x.dims())));
  out->set_dims(x.dims());
  out->share_lod(x);
  out->set_dtype(x.dtype());
}

static phi::DDim ValidateShape(const std::vector<int64_t> shape,
                               const phi::DDim& in_dims) {
  const int64_t in_size = phi::product(in_dims);
  auto in_dims_vec = phi::vectorize(in_dims);
  bool all_positive = std::all_of(in_dims_vec.cbegin(),
                                  in_dims_vec.cend(),
                                  [](int64_t i) { return i > 0; });
  // only one dimension can be set to -1, whose size will be automatically
  // infered.
  const int64_t unk_dim_val = -1;
  const int64_t copy_dim_val = 0;

  std::vector<int64_t> output_shape(shape.size(), 0);
  int64_t capacity = 1;
  int unk_dim_idx = -1;
  for (size_t i = 0; i < shape.size(); ++i) {
    if (shape[i] == unk_dim_val) {
      PADDLE_ENFORCE_EQ(
          unk_dim_idx,
          -1,
          phi::errors::InvalidArgument(
              "Only one dimension value of 'shape' in ReshapeOp can "
              "be -1. But received shape = [%s], shape[%d] is also -1.",
              phi::make_ddim(shape),
              i));
      unk_dim_idx = i;
    } else if (shape[i] == copy_dim_val) {
      PADDLE_ENFORCE_LT(
          static_cast<int>(i),
          in_dims.size(),
          phi::errors::InvalidArgument(
              "The index of 0 in `shape` must be less than "
              "the input tensor X's dimensions. "
              "But received shape = [%s], shape[%d] = 0, X's shape = [%s], "
              "X's dimensions = %d.",
              phi::make_ddim(shape),
              i,
              in_dims,
              in_dims.size()));
    } else {
      PADDLE_ENFORCE_GT(
          shape[i],
          0,
          phi::errors::InvalidArgument(
              "Each dimension value of 'shape' in ReshapeOp must not "
              "be negative except one unknown dimension. "
              "But received  shape = [%s], shape[%d] = %d.",
              phi::make_ddim(shape),
              i,
              shape[i]));
    }

    // NOTE all non-zero values will be converted to True (include negative
    // value)
    capacity *= (shape[i] ? shape[i] : in_dims[i]);
    output_shape[i] = (shape[i] ? static_cast<int64_t>(shape[i]) : in_dims[i]);
  }

  if (unk_dim_idx != -1) {
    if (all_positive) {
      // in_size < 0 and is un-determinate in compile time, skip the check,
      // for example, in_dims = [-1, 8, 1, 1], shape = [-1, 3, 8],
      // capacity = -24, in_size = -8, output_shape[0] = 0
      // the following check will fail.
      output_shape[unk_dim_idx] = -in_size / capacity;
      PADDLE_ENFORCE_EQ(
          output_shape[unk_dim_idx] * capacity,
          -in_size,
          phi::errors::InvalidArgument(
              "The 'shape' attribute in ReshapeOp is invalid. "
              "The input tensor X'size must be divisible by known "
              "capacity of 'shape'. "
              "But received X's shape = [%s], X's size = %d, "
              "'shape' is [%s], known capacity of 'shape' is %d.",
              in_dims,
              in_size,
              phi::make_ddim(shape),
              capacity));
    } else {
      output_shape[unk_dim_idx] = -1;
    }
  } else {
    if (all_positive) {
      PADDLE_ENFORCE_EQ(
          capacity,
          in_size,
          phi::errors::InvalidArgument(
              "The 'shape' in ReshapeOp is invalid. "
              "The input tensor X'size must be equal to the capacity of "
              "'shape'. "
              "But received X's shape = [%s], X's size = %d, 'shape' is "
              "[%s], the capacity of 'shape' is %d.",
              in_dims,
              in_size,
              phi::make_ddim(shape),
              capacity));
    }
  }

  // support reshape with zero-input(input tensor with product(shape) == 0)
  // by now we require that if the input tensor is zero shape, the target
  // shape of output must be zero
  if (in_size == 0) {
    PADDLE_ENFORCE_LE(
        capacity,
        in_size,
        phi::errors::InvalidArgument(
            "The 'shape' in ReshapeOp is invalid. "
            "The input tensor X's shape = [%s], X's capacity = %d."
            "But the target shape of Out is [%s],  the "
            "capacity of 'Out' is %d.",
            in_dims,
            in_size,
            phi::make_ddim(shape),
            capacity));
  }

  return phi::make_ddim(output_shape);
}

void InferMetaFromVecValue(const MetaTensor& x,
                           const std::vector<int64_t>& shape,
                           MetaTensor* out) {
  PADDLE_ENFORCE_EQ(!shape.empty(),
                    true,
                    phi::errors::InvalidArgument(
                        "The parameter 'shape' in ReshapeOp must be set. "
                        "But received 'shape' is empty."));
  auto x_dims = x.dims();
  auto out_dims = ValidateShape(shape, x_dims);
  out->set_dims(out_dims);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
  if (x_dims[0] == out_dims[0]) {
    // Only pass LoD when the first dimension of output and Input(X)
    // are the same.
    out->share_lod(x);
  }
}

void IsEmptyInferMeta(const MetaTensor& x, MetaTensor* out) {
  out->set_dims(phi::make_ddim({1}));
  out->set_dtype(DataType::BOOL);
}

void IsfiniteInferMeta(const MetaTensor& x, MetaTensor* out) {
  out->set_dims(x.dims());
  out->set_dtype(DataType::BOOL);
}

void MultinomialInferMeta(const MetaTensor& x,
                          int num_samples,
                          bool replacement,
                          MetaTensor* out) {
  auto x_dim = x.dims();
  int64_t x_rank = x_dim.size();
  PADDLE_ENFORCE_GT(x_rank,
                    0,
                    errors::InvalidArgument(
                        "The number of dimensions of the input probability "
                        "distribution should be > 0, but got %d.",
                        x_rank));
  PADDLE_ENFORCE_LE(x_rank,
                    2,
                    errors::InvalidArgument(
                        "The number of dimensions of the input probability "
                        "distribution should be <= 2, but got %d.",
                        x_rank));

  std::vector<int64_t> out_dims(x_rank);
  for (int64_t i = 0; i < x_rank - 1; i++) {
    out_dims[i] = x_dim[i];
  }

  PADDLE_ENFORCE_GT(
      num_samples,
      0,
      errors::InvalidArgument(
          "The number of samples should be > 0, but got %d.", num_samples));
  out_dims[x_rank - 1] = num_samples;

  out->set_dims(make_ddim(out_dims));
  out->set_dtype(DataType::INT64);
}

void PadInferMeta(const MetaTensor& input,
                  const std::vector<int>& paddings,
                  float pad_value,
                  MetaTensor* out,
                  MetaConfig config) {
  auto x_dim = input.dims();
  PADDLE_ENFORCE_EQ(
      static_cast<int>(paddings.size()),
      x_dim.size() * 2,
      phi::errors::InvalidArgument(
          "Size of 'paddings' dimension should be equal to 2 * size of "
          "Input(X)'s dimension, but received (size of 'paddings' dimension "
          "is) %d vs (2 * size of Input(X)'s dimension is) %d.",
          static_cast<int>(paddings.size()),
          x_dim.size() * 2));
  for (size_t i = 0; i < paddings.size(); ++i) {
    PADDLE_ENFORCE_GE(paddings[i],
                      0,
                      phi::errors::InvalidArgument(
                          "The element of 'paddings' should >= 0, but "
                          "received %d for index %d.",
                          paddings[i],
                          static_cast<int>(i)));
  }
  std::vector<int64_t> out_dims(x_dim.size());
  for (int i = 0; i < x_dim.size(); ++i) {
    if ((!config.is_runtime) && (x_dim[i] == -1)) {
      out_dims[i] = -1;
    } else {
      out_dims[i] = x_dim[i] + paddings[i * 2] + paddings[i * 2 + 1];
    }
  }
  out->set_dims(phi::make_ddim(out_dims));
  if (out_dims[0] == x_dim[0]) {
    // Only pass LoD when the first dimension is equal between
    // output and input.
    out->share_lod(input);
  }
  out->set_dtype(input.dtype());
}

void PixelShuffleInferMeta(const MetaTensor& x,
                           int upscale_factor,
                           const std::string& data_format,
                           MetaTensor* out) {
  auto input_dims = x.dims();
  PADDLE_ENFORCE_EQ(input_dims.size(),
                    4,
                    phi::errors::InvalidArgument(
                        "Input should be a 4-D tensor of format [N, C, H, W] "
                        "or [N, H, W, C], but got %u.",
                        input_dims.size()));

  const bool channel_last = (data_format == "NHWC");

  if (!channel_last) {
    PADDLE_ENFORCE_EQ(input_dims[1] % (upscale_factor * upscale_factor),
                      0,
                      phi::errors::InvalidArgument(
                          "The square of upscale_factor[%u] should divide the "
                          "number of channel[%u]",
                          upscale_factor * upscale_factor,
                          input_dims[1]));
  } else {
    PADDLE_ENFORCE_EQ(input_dims[3] % (upscale_factor * upscale_factor),
                      0,
                      phi::errors::InvalidArgument(
                          "The square of upscale_factor[%u] should divide the "
                          "number of channel[%u]",
                          upscale_factor * upscale_factor,
                          input_dims[3]));
  }
  auto output_dims = input_dims;
  output_dims[0] = input_dims[0];
  if (!channel_last) {
    output_dims[1] = input_dims[1] / (upscale_factor * upscale_factor);
    output_dims[2] = input_dims[2] * upscale_factor;
    output_dims[3] = input_dims[3] * upscale_factor;
  } else {
    output_dims[1] = input_dims[1] * upscale_factor;
    output_dims[2] = input_dims[2] * upscale_factor;
    output_dims[3] = input_dims[3] / (upscale_factor * upscale_factor);
  }
  out->set_dtype(x.dtype());
  out->set_dims(output_dims);
}

void RealAndImagInferMeta(const MetaTensor& x, MetaTensor* out) {
  out->set_dims(x.dims());
  out->set_dtype(dtype::ToReal(x.dtype()));
  out->set_layout(x.layout());
}

DDim ReduceInferDim(const MetaTensor& x,
                    const std::vector<int64_t>& axis,
                    bool keep_dim,
                    bool reduce_all) {
  auto x_rank = x.dims().size();

  std::vector<int64_t> formated_axis = axis;
  for (size_t i = 0; i < axis.size(); ++i) {
    PADDLE_ENFORCE_LT(axis[i],
                      x_rank,
                      errors::InvalidArgument(
                          "The reduce dim index %d should be in the "
                          "range [-dimension(X), dimension(X)] "
                          "which dimesion = %d. But received dim index = %d.",
                          i,
                          x_rank,
                          axis[i]));
    PADDLE_ENFORCE_GE(axis[i],
                      -x_rank,
                      errors::InvalidArgument(
                          "The reduce dim index %d should be in the "
                          "range [-dimension(X), dimension(X)] "
                          "which dimesion = %d. But received dim index = %d.",
                          i,
                          x_rank,
                          axis[i]));

    if (axis[i] < 0) {
      formated_axis[i] = axis[i] + x_rank;
    }
  }

  bool full_dim = true;
  std::set<int64_t> dims_set(formated_axis.begin(), formated_axis.end());
  for (int64_t i = 0; i < x.dims().size(); ++i) {
    if (dims_set.find(i) == dims_set.end()) {
      full_dim = false;
      break;
    }
  }
  reduce_all = reduce_all || full_dim;

  std::vector<int64_t> out_dim_vector;
  if (keep_dim) {
    for (int64_t i = 0; i < x.dims().size(); ++i) {
      if (reduce_all || dims_set.find(i) != dims_set.end()) {
        out_dim_vector.push_back(1);
      } else {
        out_dim_vector.push_back(x.dims().at(i));
      }
    }
  } else {
    for (int64_t i = 0; i < x.dims().size(); ++i) {
      if (reduce_all || dims_set.find(i) != dims_set.end()) {
        continue;
      } else {
        out_dim_vector.push_back(x.dims().at(i));
      }
    }

    if (out_dim_vector.size() == 0) {
      out_dim_vector.push_back(1);
    }
  }
  DDim out_dim = phi::make_ddim(out_dim_vector);

  return out_dim;
}

void ReduceInferMeta(const MetaTensor& x,
                     const std::vector<int64_t>& axis,
                     bool keep_dim,
                     MetaTensor* out) {
  bool reduce_all = false;
  ReduceInferMetaBase(x, axis, keep_dim, reduce_all, out);
}

void ReduceInferMetaBase(const MetaTensor& x,
                         const std::vector<int64_t>& axis,
                         bool keep_dim,
                         bool reduce_all,
                         MetaTensor* out) {
  DDim out_dim = ReduceInferDim(x, axis, keep_dim, reduce_all);
  out->set_dims(out_dim);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
}

void ReshapeInferMeta(const MetaTensor& x,
                      const ScalarArray& shape,
                      MetaTensor* out,
                      MetaConfig config) {
  auto& shape_data = shape.GetData();
  PADDLE_ENFORCE_NOT_NULL(out,
                          phi::errors::InvalidArgument(
                              "Output(Out) of ReshapeOp should not be null."));
  if (!config.is_runtime && shape.FromTensor()) {
    out->set_dims(phi::make_ddim(shape_data));
    out->share_lod(x);
    return;
  }
  PADDLE_ENFORCE_GT(shape_data.size(),
                    0,
                    phi::errors::InvalidArgument(
                        "The shape's size in ReshapeOp can't be zero."));
  InferMetaFromVecValue(x, shape_data, out);
}

void ReshapeWithXShapeInferMeta(const MetaTensor& x,
                                const ScalarArray& shape,
                                MetaTensor* xshape,
                                MetaTensor* out,
                                MetaConfig config) {
  PADDLE_ENFORCE_NOT_NULL(
      xshape,
      phi::errors::InvalidArgument(
          "Output(XShape) of ReshapeOp should not be null."));
  const auto& x_dims = x.dims();
  std::vector<int64_t> xshape_dims(x_dims.size() + 1);
  xshape_dims[0] = 0;
  for (int i = 0; i < x_dims.size(); ++i) {
    xshape_dims[i + 1] = x_dims[i];
  }
  xshape->set_dims(phi::make_ddim(xshape_dims));
  xshape->share_lod(x);
  ReshapeInferMeta(x, shape, out, config);
}

void ShardIndexInferMeta(const MetaTensor& in,
                         int index_num,
                         int nshards,
                         int shard_id,
                         int ignore_value,
                         MetaTensor* out,
                         MetaConfig config) {
  auto x_dims = in.dims();
  PADDLE_ENFORCE_GE(
      x_dims.size(),
      2,
      phi::errors::InvalidArgument("Rank of Input(X) should be at least 2, "
                                   "but the value given is %d.",
                                   x_dims.size()));
  if (config.is_runtime || x_dims[x_dims.size() - 1] > 0) {
    PADDLE_ENFORCE_EQ(x_dims[x_dims.size() - 1],
                      1U,
                      phi::errors::InvalidArgument(
                          "The last dimension of Input(X) should be 1, "
                          "but the value given is %d.",
                          x_dims[x_dims.size() - 1]));
  }

  out->set_dims(x_dims);
  out->share_lod(in);
  out->set_dtype(in.dtype());
}

void SizeInferMeta(const MetaTensor& input, MetaTensor* out) {
  out->set_dtype(DataType::INT64);
  out->set_dims({1});
}

void SoftmaxInferMeta(const MetaTensor& x, int axis, MetaTensor* out) {
  auto dim_x = x.dims();
  auto rank_x = dim_x.size();
  PADDLE_ENFORCE_GE(axis,
                    -rank_x,
                    phi::errors::InvalidArgument(
                        "Attr(axis) value should be in range [-R, R-1], "
                        "R is the rank of Input(X)."));
  PADDLE_ENFORCE_LT(axis,
                    rank_x,
                    phi::errors::InvalidArgument(
                        "Attr(axis) value should be in range [-R, R-1], "
                        "R is the rank of Input(X)."));

  out->set_dims(x.dims());
  out->set_dtype(x.dtype());
  out->share_lod(x);
}

void SplitInferMeta(const MetaTensor& x,
                    const ScalarArray& num_or_sections,
                    const Scalar& axis,
                    std::vector<MetaTensor*> out,
                    MetaConfig config) {
  int axis_value = axis.to<int>();
  int rank = x.dims().size();
  PADDLE_ENFORCE_EQ(
      axis_value >= -rank && axis_value < rank,
      true,
      phi::errors::InvalidArgument(
          "The axis is expected to be in range of [%d, %d), but got %d",
          -rank,
          rank,
          axis_value));
  if (axis_value < 0) {
    axis_value = axis_value + rank;
  }

  auto input_axis_dim = x.dims().at(axis_value);
  auto num_or_sections_data = num_or_sections.GetData();
  // step1: get formated sections
  std::vector<int64_t> sections;
  // num_or_sections is a number
  if (num_or_sections_data.size() == 1) {
    int num = num_or_sections_data.at(0);

    PADDLE_ENFORCE_EQ(input_axis_dim % num,
                      0,
                      phi::errors::InvalidArgument(
                          "The input's size along the split dimension "
                          "must be evenly divisible by Attr(num_or_sections). "
                          "But received Attr(num_or_sections) "
                          "= %d, input(X)'s shape = [%s], Attr(dim) = %d.",
                          num,
                          x.dims(),
                          axis_value));

    for (int i = 0; i < num; ++i) {
      sections.push_back(input_axis_dim / num);
    }
  } else {
    // num_or_sections is a sections
    const int unknow_dim_val = -1;
    int unknow_dim_idx = -1;
    int num_of_unknow = 0;
    int sum_of_section = 0;

    for (size_t i = 0; i < num_or_sections_data.size(); ++i) {
      sections.push_back(num_or_sections_data[i]);

      if (num_or_sections_data[i] == unknow_dim_val) {
        num_of_unknow++;
        unknow_dim_idx = i;
      } else {
        sum_of_section += num_or_sections_data[i];
      }
    }

    if (config.is_runtime) {
      PADDLE_ENFORCE_LE(num_of_unknow,
                        1,
                        phi::errors::InvalidArgument(
                            "Only one dimension value of Attr(num_or_sections) "
                            "in SplitOp can be -1. "
                            "But received Attr(num_or_sections) = [%s].",
                            phi::make_ddim(num_or_sections_data)));
    }

    if (unknow_dim_idx != -1) {
      // for example, input shape = [4 ,5], axis = 1, sections = [2, 3, -1].
      // input_axis_dim = 5, sum_of_sections = 5.
      // the following check will fail.
      PADDLE_ENFORCE_LT(
          sum_of_section,
          input_axis_dim,
          phi::errors::InvalidArgument(
              "Sum of Attr(num_or_sections) other than unknown section "
              "must be less than the input's "
              "size "
              "along the split dimension. But received Attr(num_or_sections) "
              "= [%s], input(X)'s shape = [%s], Attr(dim) = %d.",
              phi::make_ddim(num_or_sections_data),
              x.dims(),
              axis_value));

      if (config.is_runtime) {
        sections[unknow_dim_idx] = input_axis_dim - sum_of_section;
      }
    } else {
      PADDLE_ENFORCE_EQ(
          sum_of_section,
          input_axis_dim,
          phi::errors::InvalidArgument(
              "Sum of Attr(num_or_sections) must be equal to the input's "
              "size "
              "along the split dimension. But received Attr(num_or_sections)"
              " = [%s], input(X)'s shape = [%s], Attr(dim) = %d.",
              phi::make_ddim(num_or_sections_data),
              x.dims(),
              axis_value));
    }
  }

  // setp2: fill out dims
  std::vector<phi::DDim> out_dims(sections.size(), x.dims());
  if (config.is_runtime || input_axis_dim > 0) {
    for (size_t i = 0; i < sections.size(); ++i) {
      out_dims[i][axis_value] = sections[i];
    }
  } else {
    for (size_t i = 0; i < sections.size(); ++i) {
      out_dims[i][axis_value] = -1;
    }
  }

  for (size_t i = 0; i < sections.size(); ++i) {
    if (axis_value != 0) {
      // Only pass LoD when not spliting along the first dim.
      out[i]->set_dtype(x.dtype());
      out[i]->set_dims(out_dims[i]);
      out[i]->set_layout(x.layout());
    } else {
      out[i]->set_dtype(x.dtype());
      out[i]->set_dims(out_dims[i]);
      out[i]->set_layout(x.layout());
      out[i]->share_lod(x);
    }
  }
}

/*  Why not use SumRawInferMeta directly?
    Because we need make InferMetaFunction's args follow the design of api.yaml
*/
void SumInferMeta(const MetaTensor& x,
                  const std::vector<int64_t>& axis,
                  DataType dtype,
                  bool keep_dim,
                  MetaTensor* out) {
  bool reduce_all = false;
  SumRawInferMeta(x, axis, keep_dim, reduce_all, dtype, out);
}

void SumRawInferMeta(const MetaTensor& x,
                     const std::vector<int64_t>& axis,
                     bool keep_dim,
                     bool reduce_all,
                     DataType dtype,
                     MetaTensor* out) {
  DDim out_dim = ReduceInferDim(x, axis, keep_dim, reduce_all);

  DataType out_dtype;
  if (dtype != DataType::UNDEFINED) {
    out_dtype = dtype;
  } else {
    if (x.dtype() == DataType::BOOL || x.dtype() == DataType::INT32 ||
        x.dtype() == DataType::INT64) {
      out_dtype = DataType::INT64;
    } else {
      out_dtype = x.dtype();
    }
  }

  out->set_dims(out_dim);
  out->set_dtype(out_dtype);
  out->set_layout(x.layout());
}

void TileInferMeta(const MetaTensor& x,
                   const ScalarArray& repeat_times,
                   MetaTensor* out,
                   MetaConfig config) {
#define MAX_RANK_SUPPORTED 6

  auto repeat_times_data = repeat_times.GetData();
  auto x_dims = x.dims();
  if (repeat_times_data.size() == 0) {
    repeat_times_data = std::vector<int64_t>(x_dims.size(), -1);
  }

  PADDLE_ENFORCE_LE(
      x_dims.size(),
      MAX_RANK_SUPPORTED,
      errors::InvalidArgument(
          "The rank of the input 'x' for tile op "
          "must not be greater than %d, but the value received is %d.",
          MAX_RANK_SUPPORTED,
          x_dims.size()));
  PADDLE_ENFORCE_LE(
      repeat_times_data.size(),
      MAX_RANK_SUPPORTED,
      errors::InvalidArgument(
          "The size of the shape of input 'repeat_times' for tile op "
          "must not be greater than %d, but the value received is %d.",
          MAX_RANK_SUPPORTED,
          repeat_times_data.size()));
  PADDLE_ENFORCE_GE(
      repeat_times_data.size(),
      1,
      errors::InvalidArgument(
          "The size of the shape of input 'repeat_times' for tile op "
          "must be positive integers, but the value received is %d.",
          repeat_times_data.size()));

  auto out_rank =
      std::max(static_cast<size_t>(x_dims.size()), repeat_times_data.size());
  std::vector<int64_t> out_shape(out_rank);
  auto x_dim_vec = phi::vectorize<int>(x_dims);
  if (x_dim_vec.size() > repeat_times_data.size()) {
    auto diff = x_dim_vec.size() - repeat_times_data.size();
    repeat_times_data.insert(repeat_times_data.begin(), diff, -1);
  } else {
    auto diff = repeat_times_data.size() - x_dim_vec.size();
    x_dim_vec.insert(x_dim_vec.begin(), diff, -1);
  }
  for (size_t i = 0; i < repeat_times_data.size(); ++i) {
    if (x_dim_vec[i] == -1 || repeat_times_data[i] == -1) {
      out_shape[i] = -1;
    } else {
      PADDLE_ENFORCE_GT(
          repeat_times_data[i],
          0,
          errors::InvalidArgument(
              "Every element of the input 'repeat_times' for tile op must be "
              "greater than 0, but the value given is %d.",
              repeat_times_data[i]));
      out_shape[i] = x_dim_vec[i] * repeat_times_data[i];
    }
  }

  out->set_dims(phi::make_ddim(out_shape));
  if (out_shape[0] == x_dims[0]) {
    out->share_lod(x);
  }
}

void UnStackInferMeta(const MetaTensor& x,
                      int axis,
                      int num,
                      std::vector<MetaTensor*> outs) {
  auto x_dim = x.dims();
  int rank = x_dim.size();
  PADDLE_ENFORCE_GE(axis,
                    -rank,
                    phi::errors::InvalidArgument(
                        "The attribute axis is out of range, it must be "
                        "inside [-rank, rank), where rank = %d",
                        rank));
  PADDLE_ENFORCE_LT(axis,
                    rank,
                    phi::errors::InvalidArgument(
                        "The attribute axis is out of range, it must be "
                        "inside [-rank, rank), where rank = %d",
                        rank));
  if (axis < 0) axis += rank;

  size_t output_count = outs.size();
  PADDLE_ENFORCE_EQ(output_count,
                    static_cast<size_t>(num),
                    phi::errors::InvalidArgument(
                        "Number of Outputs(Y) is wrong. Got %d , but it must "
                        "equal to attribute num which is %d.",
                        output_count,
                        static_cast<size_t>(num)));
  if (x_dim[axis] > 0) {
    PADDLE_ENFORCE_EQ(
        num,
        x_dim[axis],
        phi::errors::InvalidArgument(
            "The number of attribute num is not equal to the length of the "
            "%d axis of Input(X). Expect %d but got %d.",
            axis,
            x_dim[axis],
            num));
  }
  auto vec = phi::vectorize<int>(x_dim);
  vec.erase(vec.begin() + axis);
  for (size_t i = 0; i < output_count; i++) {
    outs[i]->set_dims(phi::make_ddim(vec));
    outs[i]->set_dtype(x.dtype());
  }
}

void TraceInferMeta(
    const MetaTensor& x, int offset, int axis1, int axis2, MetaTensor* out) {
  int dim1 = axis1;
  int dim2 = axis2;

  auto x_dims = x.dims();

  int dim1_ = dim1 < 0 ? x_dims.size() + dim1 : dim1;
  int dim2_ = dim2 < 0 ? x_dims.size() + dim2 : dim2;

  PADDLE_ENFORCE_GE(
      x_dims.size(),
      2,
      phi::errors::OutOfRange(
          "Input's dim is out of range (expected at least 2, but got %ld).",
          x_dims.size()));
  PADDLE_ENFORCE_LT(
      dim1_,
      x_dims.size(),
      phi::errors::OutOfRange(
          "Attr(dim1) is out of range (expected to be in range of [%ld, "
          "%ld], but got %ld).",
          -(x_dims.size()),
          (x_dims.size() - 1),
          dim1));
  PADDLE_ENFORCE_LT(
      dim2_,
      x_dims.size(),
      phi::errors::OutOfRange(
          "Attr(dim2) is out of range (expected to be in range of [%ld, "
          "%ld], but got %ld).",
          -(x_dims.size()),
          (x_dims.size() - 1),
          dim2));
  PADDLE_ENFORCE_NE(
      dim1_,
      dim2_,
      phi::errors::InvalidArgument("The dimensions should not be identical "
                                   "%ld vs %ld.",
                                   dim1,
                                   dim2));

  auto sizes = vectorize(x_dims);
  if (x_dims.size() == 2) {
    sizes.clear();
    sizes.push_back(1);
  } else {
    sizes.erase(sizes.begin() + std::max(dim1_, dim2_));
    sizes.erase(sizes.begin() + std::min(dim1_, dim2_));
  }
  out->set_dims(phi::make_ddim(sizes));
  out->set_dtype(x.dtype());
}

void TransferLayoutInferMeta(const MetaTensor& x,
                             DataLayout layout,
                             MetaTensor* out) {
  out->set_dims(x.dims());
  out->set_dtype(x.dtype());
  out->set_layout(layout);
}

void TransposeInferMeta(const MetaTensor& x,
                        const std::vector<int>& axis,
                        MetaTensor* out) {
  auto x_dims = x.dims();
  size_t x_rank = x_dims.size();
  size_t axis_size = axis.size();

  PADDLE_ENFORCE_EQ(
      x_rank,
      axis_size,
      errors::InvalidArgument("The input tensor's dimension "
                              "should be equal to the axis's size. "
                              "But received input tensor's dimension is %d, "
                              "axis's size is %d",
                              x_rank,
                              axis_size));

  std::vector<int> count(axis_size, 0);
  for (size_t i = 0; i < axis_size; i++) {
    PADDLE_ENFORCE_GE(
        axis[i],
        0,
        errors::InvalidArgument("The axis should be greater than or equal to 0."
                                "But received %d of axis[%d]",
                                axis[i],
                                i));

    PADDLE_ENFORCE_EQ(
        axis[i] < static_cast<int>(axis_size) && ++count[axis[i]] == 1,
        true,
        errors::InvalidArgument(
            "Each element of Attribute axis should "
            "be a unique value range from 0 to (dims - 1), "
            "where the dims is the axis's size, "
            "unique value means this axis value can appear only once. "
            "But received axis[%d] is %d, axis_size is %d, "
            "count[axis[%d]] is %d",
            i,
            axis[i],
            axis_size,
            i,
            count[axis[i]]));
  }

  phi::DDim out_dims(x_dims);
  for (size_t i = 0; i < axis_size; ++i) {
    out_dims[i] = x_dims[axis[i]];
  }

  out->set_dims(out_dims);
  out->set_dtype(x.dtype());
}

void UnbindInferMeta(const MetaTensor& x,
                     int axis,
                     std::vector<MetaTensor>* outs) {
  auto in_dims = x.dims();
  std::vector<int> out_dim;
  axis = axis < 0 ? in_dims.size() + axis : axis;
  for (int i = 0; i < in_dims.size(); ++i) {
    if (i != axis) out_dim.push_back(in_dims[i]);
  }
  auto out_dims = phi::make_ddim(out_dim);

  for (size_t i = 0; i < outs->size(); ++i) {
    (*outs)[i].set_dtype(x.dtype());
    (*outs)[i].set_dims(out_dims);
    (*outs)[i].set_layout(x.layout());
    (*outs)[i].share_lod(x);
  }
}

void UnchangedInferMeta(const MetaTensor& x, MetaTensor* out) {
  out->share_meta(x);
}

// meta x -> out without change, check if axis in range [-Rank(x), Rank(x)-1]
void UnchangedInferMetaCheckAxis(const MetaTensor& x,
                                 int axis,
                                 MetaTensor* out) {
  auto rank = x.dims().size();
  PADDLE_ENFORCE_GE(
      axis,
      -rank,
      errors::InvalidArgument(
          "Attr(axis) value should be in range [-R, R-1], "
          "R is the rank of Input(X). But received axis: %d, R: %d.",
          axis,
          rank));
  PADDLE_ENFORCE_LT(
      axis,
      rank,
      phi::errors::InvalidArgument(
          "Attr(axis) value should be in range [-R, R-1], "
          "R is the rank of Input(X). But received axis: %d, R: %d.",
          axis,
          rank));
  out->share_meta(x);
}

void UnfoldInferMeta(const MetaTensor& x,
                     const std::vector<int>& kernel_sizes,
                     const std::vector<int>& strides,
                     const std::vector<int>& paddings,
                     const std::vector<int>& dilations,
                     MetaTensor* out,
                     MetaConfig config) {
  auto in_dims = x.dims();
  // Only [N, C, H, W] input supported now
  PADDLE_ENFORCE_EQ(
      in_dims.size(),
      4,
      phi::errors::InvalidArgument(
          "Input should be 4-D tensor of format [N, C, H, W], but get %u",
          in_dims.size()));
  PADDLE_ENFORCE_EQ(
      in_dims.size() - kernel_sizes.size(),
      2U,
      phi::errors::InvalidArgument(
          "The dims of X should be larger than that of kernel_sizes "
          "by a number of 2, due to the batch size and input channel dim. "
          "But recieved dims(X:%u) - dims(kernel_sizes:%u) != 2",
          in_dims.size(),
          kernel_sizes.size()));
  PADDLE_ENFORCE_EQ(
      strides.size(),
      kernel_sizes.size(),
      phi::errors::InvalidArgument(
          "The dims of strides should be the same with that of kernel_sizes. "
          "But recieved dims(strides: %u) != dims(kernel_sizes: %u).",
          strides.size(),
          kernel_sizes.size()));
  PADDLE_ENFORCE_EQ(
      paddings.size(),
      2 * strides.size(),
      phi::errors::InvalidArgument(
          "The dims of paddings should be 2 times of that of strides. "
          "But recieved dims(paddings: %u) != 2*dims(strides: %u).",
          paddings.size(),
          strides.size()));
  PADDLE_ENFORCE_EQ(
      strides.size(),
      dilations.size(),
      phi::errors::InvalidArgument(
          "The dims of strides should be the same with that of dilations. "
          "But recieved dims(strides: %u) != dims(dilations: %u).",
          strides.size(),
          dilations.size()));

  // check kernel_sizes
  PADDLE_ENFORCE_GT(kernel_sizes[0],
                    0,
                    phi::errors::InvalidArgument(
                        "The `kernel_sizes` should be greater than zero, "
                        "but recieved kernel_height: %d kernel_width: %d.",
                        kernel_sizes[0],
                        kernel_sizes[1]));
  PADDLE_ENFORCE_GT(kernel_sizes[1],
                    0,
                    phi::errors::InvalidArgument(
                        "The `kernel_sizes` should be greater than zero, "
                        "but recieved kernel_height: %d kernel_width: %d.",
                        kernel_sizes[0],
                        kernel_sizes[1]));
  // check strides
  PADDLE_ENFORCE_GT(strides[0],
                    0,
                    phi::errors::InvalidArgument(
                        "The `strides` should be greater than zero, "
                        "but recieved strides_height: %d strides_width: %d.",
                        strides[0],
                        strides[1]));
  PADDLE_ENFORCE_GT(strides[1],
                    0,
                    phi::errors::InvalidArgument(
                        "The `strides` should be greater than zero, "
                        "but recieved strides_height: %d strides_width: %d.",
                        strides[0],
                        strides[1]));
  // check dilations
  PADDLE_ENFORCE_GT(
      dilations[0],
      0,
      phi::errors::InvalidArgument(
          "The `dilations` should be greater than zero, "
          "but recieved dilations_height: %d dilations_width: %d.",
          dilations[0],
          dilations[1]));
  PADDLE_ENFORCE_GT(
      dilations[1],
      0,
      phi::errors::InvalidArgument(
          "The `dilations` should be greater than zero, "
          "but recieved dilations_height: %d dilations_width: %d.",
          dilations[0],
          dilations[1]));

  std::vector<int> out_dims;
  out_dims.push_back(in_dims[0]);
  int output_channels = in_dims[1] * kernel_sizes[0] * kernel_sizes[1];
  out_dims.push_back(output_channels);

  int output_height = phi::funcs::CalcOutputSize(in_dims[2],
                                                 kernel_sizes[0],
                                                 dilations[0],
                                                 paddings[0],
                                                 paddings[2],
                                                 strides[0]);
  int output_width = phi::funcs::CalcOutputSize(in_dims[3],
                                                kernel_sizes[1],
                                                dilations[1],
                                                paddings[1],
                                                paddings[3],
                                                strides[1]);
  if (config.is_runtime) {
    // only check output height and width in runtime
    PADDLE_ENFORCE_GT(
        output_height,
        0,
        phi::errors::InvalidArgument(
            "The sliding blocks calculated from input spatial size "
            "(%d, %d), kernel_sizes (%d, %d), strides (%d, %d), "
            "dilations (%d, %d), is (%d, %d), which should be a "
            "positive integer.",
            in_dims[2],
            in_dims[3],
            kernel_sizes[0],
            kernel_sizes[1],
            strides[0],
            strides[1],
            dilations[0],
            dilations[1],
            output_height,
            output_width));
    PADDLE_ENFORCE_GT(
        output_width,
        0,
        phi::errors::InvalidArgument(
            "The sliding blocks calculated from input spatial size "
            "(%d, %d), kernel_sizes (%d, %d), strides (%d, %d), "
            "dilations (%d, %d), is (%d, %d), which should be a "
            "positive integer.",
            in_dims[2],
            in_dims[3],
            kernel_sizes[0],
            kernel_sizes[1],
            strides[0],
            strides[1],
            dilations[0],
            dilations[1],
            output_height,
            output_width));
  }
  int output_col_length = output_height * output_width;
  out_dims.push_back(output_col_length);
  out->set_dims(phi::make_ddim(out_dims));
}

void WhereIndexInferMeta(const MetaTensor& condition, MetaTensor* out) {
  auto rank = condition.dims().size();
  PADDLE_ENFORCE_GE(
      rank,
      1UL,
      phi::errors::InvalidArgument(
          "Input(Condition) should have number of dimension at least 1"));
  out->set_dims(phi::make_ddim({-1, rank}));
  out->set_dtype(DataType::INT64);
}

}  // namespace phi

PD_REGISTER_INFER_META_FN(copy_to, phi::CopyToInferMeta);
PD_REGISTER_INFER_META_FN(split, phi::SplitInferMeta);
