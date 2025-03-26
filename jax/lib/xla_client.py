# Copyright 2024 The JAX Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from jax._src.lib import xla_client as _xc

get_topology_for_devices = _xc.get_topology_for_devices
heap_profile = _xc.heap_profile
mlir_api_version = _xc.mlir_api_version
Client = _xc.Client
CompileOptions = _xc.CompileOptions
DeviceAssignment = _xc.DeviceAssignment
Frame = _xc.Frame
HloSharding = _xc.HloSharding
OpSharding = _xc.OpSharding
Traceback = _xc.Traceback

_deprecations = {
    # Finalized 2025-03-25; remove after 2025-06-25
    "FftType": (
        "jax.lib.xla_client.FftType was removed in JAX v0.6.0; use jax.lax.FftType.",
        None,
    ),
    "PaddingType": (
        (
            "jax.lib.xla_client.PaddingType was removed in JAX v0.6.0;"
            " this type is unused by JAX so there is no replacement."
        ),
        None,
    ),
    "dtype_to_etype": (
        "dtype_to_etype was removed in JAX v0.6.0; use StableHLO instead.",
        None,
    ),
    "shape_from_pyval": (
        "shape_from_pyval was removed in JAX v0.6.0; use StableHLO instead.",
        None,
    ),
    # Added Oct 11 2024
    "ops": (
        "ops is deprecated; use StableHLO instead.",
        _xc.ops,
    ),
    "register_custom_call_target": (
        "register_custom_call_target is deprecated; use the JAX FFI instead "
        "(https://jax.readthedocs.io/en/latest/ffi.html)",
        _xc.register_custom_call_target,
    ),
    "PrimitiveType": (
        "PrimitiveType is deprecated; use StableHLO instead.",
        _xc.PrimitiveType,
    ),
    "Shape": (
        "Shape is deprecated; use StableHLO instead.",
        _xc.Shape,
    ),
    "XlaBuilder": (
        "XlaBuilder is deprecated; use StableHLO instead.",
        _xc.XlaBuilder,
    ),
    "XlaComputation": (
        "XlaComputation is deprecated; use StableHLO instead.",
        _xc.XlaComputation,
    ),
    # Added Nov 20 2024
    "ArrayImpl": (
        "jax.lib.xla_client.ArrayImpl is deprecated; use jax.Array instead.",
        _xc.ArrayImpl,
    ),
}

import typing as _typing

if _typing.TYPE_CHECKING:
  ops = _xc.ops
  register_custom_call_target = _xc.register_custom_call_target
  ArrayImpl = _xc.ArrayImpl
  PrimitiveType = _xc.PrimitiveType
  Shape = _xc.Shape
  XlaBuilder = _xc.XlaBuilder
  XlaComputation = _xc.XlaComputation
else:
  from jax._src.deprecations import deprecation_getattr as _deprecation_getattr

  __getattr__ = _deprecation_getattr(__name__, _deprecations)
  del _deprecation_getattr
del _typing
del _xc
