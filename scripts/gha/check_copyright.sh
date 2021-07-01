# Copyright 2021 Google LLC
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

usage(){
    echo "Usage: $0 [options]                                                                                          
 options:                                                                                                              
   -g          enable github log format"
}

github_log=0
IFS=',' # split options on ',' characters                                                                              
while getopts "hg" opt; do
    case $opt in
        h)
            usage
            exit 0
            ;;
        g)
	    github_log=1
            ;;
        *)
            echo "unknown parameter"
            exit 2
            ;;
    esac
done


# Check source files for copyright notices

options=(
  -E  # Use extended regexps
  -I  # Exclude binary files
  -L  # Show files that don't have a match
  'Copyright.*[0-9]{4}.*Google'
)

result=$(git grep "${options[@]}" -- \
    '*.'{c,cc,cmake,h,js,m,mm,py,rb,sh,swift} \
    CMakeLists.txt '**/CMakeLists.txt' \
    ':(exclude)**/third_party/**' \
    ':(exclude)**/external/**')

if [[ $result ]]; then
    if [[ $github_log -eq 1 ]]; then
	echo -n "::error ::Missing copyright notices in the following files:%0A"
	# Print all files with %0A instead of newline.
	echo "$result" | sed ':a;N;$!ba;s/\n/%0A/g'
    else
	echo "$result"
	echo "ERROR: Missing copyright notices in the files above. Please fix."
    fi
    exit 1
fi
