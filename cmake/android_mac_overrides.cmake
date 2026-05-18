# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(APPLE AND ANDROID)
  message(STATUS "Setting CMAKE_AR and CMAKE_RANLIB overrides for Android on Mac host")
  set(CMAKE_AR "/usr/bin/ar" CACHE FILEPATH "Archive tool" FORCE)
  set(CMAKE_RANLIB "/usr/bin/ranlib" CACHE FILEPATH "Indexing tool" FORCE)
endif()
