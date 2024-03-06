# Path to the CMakeLists.txt file
$filePath = $args[0]

# Lines to add after the line starting with "cmake_minimum_required"
$newLines = @(
    'string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")',
    'string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")',
    'string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")',
    'string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")'
)

# Read the content of the file
$content = Get-Content -Path $filePath

# New content holder
$newContent = @()

# Flag to check if lines are added
$linesAdded = $false

foreach ($line in $content) {
    # Add the current line to new content
    $newContent += $line

    # Check if the line starts with "cmake_minimum_required" and add new lines after it
    if ($line -match '^cmake_minimum_required' -and -not $linesAdded) {
        $newContent += $newLines
        $linesAdded = $true
    }
}

# Write the new content back to the file
$newContent | Set-Content -Path $filePath
