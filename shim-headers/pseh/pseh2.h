/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     PSEH2 spellings mapped onto MSVC structured exception handling
 */

#ifndef _MDR_PSEH2_H
#define _MDR_PSEH2_H

#define _SEH2_TRY                   __try {
#define _SEH2_FINALLY               } __finally {
#define _SEH2_EXCEPT(...)           } __except(__VA_ARGS__) {
#define _SEH2_END                   }
#define _SEH2_LEAVE                 __leave
#define _SEH2_YIELD(stmt)           stmt
#define _SEH2_GetExceptionCode()    GetExceptionCode()
#define _SEH2_GetExceptionInformation() GetExceptionInformation()
#define _SEH2_AbnormalTermination() AbnormalTermination()

#endif /* _MDR_PSEH2_H */
