# ==============================================================================
# Firebase Standard Target Macro
# Centralizes all library definitions, platform handling, and Apple output standards!
# ==============================================================================

macro(add_firebase_sdk_target target_name apple_output_name)
  # 1. Define the static library from the provided source list
  add_library(${target_name} STATIC ${ARGN})

  # 2. Standardize Apple binary configuration globally
  if(IOS OR TVOS OR MACOS)
    set_target_properties(${target_name} PROPERTIES
      FRAMEWORK TRUE
      OUTPUT_NAME "${apple_output_name}"
    )
  endif()

  # 3. Apply common IDE folder nesting rules
  set_property(TARGET ${target_name} PROPERTY FOLDER "Firebase Cpp")

  # 4. Automatically attach the generated core headers as a standard dependency
  if(TARGET FIREBASE_APP_GENERATED_HEADERS)
    add_dependencies(${target_name} FIREBASE_APP_GENERATED_HEADERS)
  endif()
endmacro()
