# Contributing

Keep changes inside the persistence authority of this repository.

Before submitting a change:

1. run shared and static builds;
2. run `unit`, `mechanism`, `header`, and `contract` suites;
3. run ASan+UBSan;
4. run both GCC and Clang qualification; and
5. update `STORAGE.md` for any durable-format change.

Do not add package discovery, apply-store enumeration, target mutation, package
ownership, UI policy, or transaction orchestration here.

Do not parse another repository's private filenames. Cross-boundary translation
belongs in an explicit adapter repository with its own tests.
