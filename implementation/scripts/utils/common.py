#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Author: Chi Zhang <chizhang@ethz.ch>


def write_class_file(obj, filename):
    cls_name = obj.__class__.__name__
    attrs = vars(obj)  # gets all instance attributes

    with open(filename, 'w') as f:
        f.write(f"class {cls_name}:\n")
        f.write("    def __init__(self):\n")
        if not attrs:
            f.write("        pass\n")
        else:
            for key, value in attrs.items():
                f.write(f"        self.{key} = {repr(value)}\n")
    pass