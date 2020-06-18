# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

firebase_cpp_cc_test(
  firebase_rtdb_util_desktop_test
  SOURCES
    desktop/util_desktop_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_indexed_variant_test
  SOURCES
    desktop/core/indexed_variant_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_tracked_query_manager_test
  SOURCES
    desktop/core/tracked_query_manager_test.cc
    desktop/test/mock_persistence_storage_engine.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_compound_write_test
  SOURCES
    desktop/core/compound_write_test.cc
  DEPENDS
    firebase_database
    firebase_testing

)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_tree_test
  SOURCES
    desktop/core/tree_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_child_change_accumulator_test
  SOURCES
    desktop/view/child_change_accumulator_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_cache_test
  SOURCES
    desktop/view/view_cache_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_operation_test
  SOURCES
    desktop/core/operation_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_write_tree_test
  SOURCES
    desktop/core/write_tree_test.cc
    desktop/test/mock_write_tree.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_indexed_filter_test
  SOURCES
    desktop/view/indexed_filter_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_limited_filter_test
  SOURCES
    desktop/view/limited_filter_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_ranged_filter_test
  SOURCES
    desktop/view/ranged_filter_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_test
  SOURCES
    desktop/test/matchers.h
    desktop/view/view_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_matchers_test
  SOURCES
    desktop/test/matchers.h
    desktop/test/matchers_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_processor_test
  SOURCES
    desktop/view/view_processor_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_persistence_manager_test
  SOURCES
    desktop/persistence/persistence_manager_test.cc
    desktop/test/mock_cache_policy.h
    desktop/test/mock_persistence_storage_engine.h
    desktop/test/mock_tracked_query_manager.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_noop_persistence_manager_test
  SOURCES
    desktop/persistence/noop_persistence_manager_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_cache_policy_test
  SOURCES
    desktop/core/cache_policy_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_prune_forest_test
  SOURCES
    desktop/persistence/prune_forest_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_in_memory_persistence_storage_engine_test
  SOURCES
    desktop/persistence/in_memory_persistence_storage_engine_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_flatbuffer_conversion_test
  SOURCES
    desktop/persistence/flatbuffer_conversions_test.cc
  DEPENDS
    firebase_database
    firebase_testing
    flexbuffer_matcher
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_sync_point_test
  SOURCES
    desktop/core/sync_point_test.cc
    desktop/test/matchers.h
    desktop/test/mock_cache_policy.h
    desktop/test/mock_listener.h
    desktop/test/mock_persistence_manager.h
    desktop/test/mock_persistence_storage_engine.h
    desktop/test/mock_tracked_query_manager.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_sync_tree_test
  SOURCES
    desktop/core/sync_tree_test.cc
    desktop/test/mock_listen_provider.h
    desktop/test/mock_listener.h
    desktop/test/mock_persistence_manager.h
    desktop/test/mock_persistence_storage_engine.h
    desktop/test/mock_tracked_query_manager.h
    desktop/test/mock_write_tree.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_server_values_test
  SOURCES
    desktop/core/server_values_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_sparse_snapshot_tree_test
  SOURCES
    desktop/core/sparse_snapshot_tree_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_core_event_registration_test
  SOURCES
    desktop/core/event_registration_test.cc
    desktop/test/mock_listener.h
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_mutable_data_desktop_test
  SOURCES
    desktop/mutable_data_desktop_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_change_test
  SOURCES
    desktop/view/change_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_view_event_generator_test
  SOURCES
    desktop/view/event_generator_test.cc
  DEPENDS
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_common_database_reference_test
  SOURCES
    common/database_reference_test.cc
  DEPENDS
    firebase_app_for_testing
    firebase_database
    firebase_testing
)

firebase_cpp_cc_test(
  firebase_rtdb_desktop_connection_web_socket_client_impl_test
  SOURCES
    desktop/connection/web_socket_client_impl_test.cc
  INCLUDES
    ${OPENSSL_INCLUDE_DIR}
    ${UWEBSOCKETS_SOURCE_DIR}/..
  DEPENDS
    firebase_database
    firebase_testing
    ${OPENSSL_CRYPTO_LIBRARY}
    libuWS
)

if(MSVC)
  target_compile_definitions(firebase_rtdb_desktop_connection_web_socket_client_impl_test
    PRIVATE
      -DWIN32_LEAN_AND_MEAN
  )
endif()

firebase_cpp_cc_test(
  firebase_rtdb_desktop_connection_connection_test
  SOURCES
    desktop/connection/connection_test.cc
  INCLUDES
    ${OPENSSL_INCLUDE_DIR}
    ${UWEBSOCKETS_SOURCE_DIR}/..
  DEPENDS
    ${OPENSSL_CRYPTO_DIR}
    libuWS
    firebase_app_for_testing
    firebase_database
    firebase_testing
)

