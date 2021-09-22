int CU_assertImplementation (int bValue, unsigned uiLine, const char *strCondition, const char *strFile, const char *strFunction, int bFatal)
{
  if (!bValue && bFatal) {
    __coverity_panic__ ();
  }
  return bValue;
}
