read pv < <(tr '\t' ' ' < package.xml | sed -ne 's;^ *<version> *\([0-9.][0-9.]*\) *</version>;\1;p')
read cv < <(tr '\t' ' ' < CMakeLists.txt | sed -ne 's;^ *project *( *CycloneDDS .*VERSION  *\([0-9.][0-9.]*\)[ )].*;\1;p')
echo "package.xml version:    $pv"
echo "CMakeLists.txt version: $cv"
if [[ -z "$pv" || -z "$cv" ]] ; then
  echo "version extraction failed"
  exit 1
fi
if [[ "$pv" != "$cv" ]] ; then
  echo "version mismatch"
  exit 1
fi
