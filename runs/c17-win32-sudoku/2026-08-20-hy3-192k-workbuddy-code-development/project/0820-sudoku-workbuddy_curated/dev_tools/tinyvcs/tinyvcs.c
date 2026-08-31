/* tinyvcs.c - command line entry point for the tinyvcs snapshot VCS.
 *
 * All argument parsing, repository work and human readable output live in
 * tinyvcs_core.c (tv_dispatch).  This translation unit only converts the
 * Windows wide command line into the canonical exit status contract of
 * docs/19 section 3:
 *
 *   0  success
 *   2  usage error
 *   3  corrupt data / repository / config
 *   4  I/O, permission, resource or system error
 *   5  integrity verification found a problem
 *   70 unexpected internal invariant failure
 */
#include "tinyvcs_core.h"

int wmain(int argc, wchar_t **argv) {
    return tv_dispatch(argc, argv);
}
