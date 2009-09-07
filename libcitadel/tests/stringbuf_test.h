    
#ifndef CUNIT_AUTOMATED_H_SEEN
#define CUNIT_AUTOMATED_H_SEEN

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/TestDB.h>


CU_EXPORT void         CU_automated_run_tests(void);
CU_EXPORT CU_ErrorCode CU_list_tests_to_file(void);
CU_EXPORT void         CU_set_output_filename(const char* szFilenameRoot);


#endif  /*  CUNIT_AUTOMATED_H_SEEN  */
