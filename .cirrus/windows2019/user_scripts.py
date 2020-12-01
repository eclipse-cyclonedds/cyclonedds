#!/bin/python
import os
import sysconfig

user_scripts_path = sysconfig.get_path('scripts', f'{os.name}_user')
print(user_scripts_path)
