FROM cirrusci/windowsservercore:cmake
COPY .cirrus/windows2019/add_path.bat add_path.bat
COPY .cirrus/windows2019/user_scripts.py user_scripts.py

#
# Disable usePackageExitCodes to disable propagation of package exit codes
# that signal a restart is required and cause Docker build to fail.
#
RUN choco feature disable -n=usePackageExitCodes && \
    choco install --yes --no-progress --installargs='ADD_CMAKE_TO_PATH=System' python3 && \
    choco install --yes --no-progress openjdk11 maven
RUN choco install --yes --no-progress winflexbison3 || echo "ignore reboot request"

#
# Add Python directory to the user path.
# Add Git to the system path in order to use CIRRUS_SHEL: bash
#
RUN python user_scripts.py > %TEMP%\user_base && \
    add_path READ USER %TEMP%\user_base
