# Team review and completion plan

Names and IDs below are transcribed from the uploaded proposal. This is the
**planned ownership**, not verified evidence of each member's actual authorship.
The team should record real implementation, testing and review work before the
final contribution summary is submitted.

| Member | KFUPM ID | Proposal responsibility | Suggested review evidence |
|---|---|---|---|
| Yasmeen Alshehri | 202271660 | CLI, validation, task creation, inspection, block/wake | T3, T5, T7, T9; demonstrate rejected input |
| Fatimah Alshehab | 202278660 | allocation wrapper, ownership registry, budget and cleanup | T6, T10, T11; explain transactional creation |
| Raneem Alshahrani | 202277080 | ready queue, round robin, clock and metrics | T1, T4; explain READY waiting vs blocked time |
| Sarah Almas | 202261060 | notes, simulated sessions, logs and demonstration evidence | T2, T8, T12; demonstrate restart persistence |

Before the Phase 2 submission deadline of 10 November 2026:

1. 5-6 November: every member builds the delivered source on the agreed Linux/WSL
   environment and runs the tests for their assigned module.
2. 7-8 November: cross-review another module; rerun `make test`, `make sanitize`,
   and `make leak-check` on a supported host. Record actual results and any fixes.
3. 9 November: check names/IDs, verify PDF and ZIP open, confirm evidence matches
   the reviewed source, and select one group member to upload.
4. 10 November: submit one group copy of the PDF progress report plus source ZIP
   (or the reviewed GitHub repository link) through the course submission page.

For the final phase, use the three proposal scenarios: exact reference schedule
then at least five tasks; blocking/budget/forced failure; note persistence across
restart and error recovery. Finalize the design/limitations document, record the
demonstration if required for that phase, and replace planned ownership with
actual member contributions. Team-device validation and peer review are not
claimed as completed by this generated preparation package.
