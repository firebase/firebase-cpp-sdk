What is generate_xml_from_google_services_json.exe?
==================================================
A Windows executable version of generate_xml_from_google_services_json.py.
This lets Windows users run our Python script without having Python installed.

How do I create generate_xml_from_google_services_json.exe?
==========================================================
We use PyInstaller (http://www.pyinstaller.org/) to bundle the Python dll
and support libraries together into one executable.

On a Windows machine,
1. Install the latest version of Python 2.7 from
   http://www.python.org/downloads/.
2. Add the Python directory to your PATH.
3. Install `setuptools`.
   - python -m ensurepip --default-pip
4. Install PyInstaller from http://www.pyinstaller.org/.
5. Create a single exe file using PyInstaller:
   - Run
     pyinstaller.exe --onefile generate_xml_from_google_services_json.py
   - The executable generate_xml_from_google_services_json.exe is output
     to the `dist` directory.

Why PyInstaller?
===============
There are a lot of Python-to-executable bundlers. PyInstaller appears to have
the best platform support. It works on Mac and Linux as well as Windows.
It's also the most popular in the community, from what I can tell.
