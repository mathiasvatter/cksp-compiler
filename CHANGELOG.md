# Changelog

## [0.0.9-alpha.8]

>[!IMPORTANT]
>This alpha release provides a hotfix for an issue (that was not fixed in alpha.7) with thread unsafe variables causing Kontakt performance issues

### Fixed

- Fixed regression issue [#110](https://github.com/mathiasvatter/cksp-compiler/issues/110), where function parameters were incorrectly marked as **thread-safe**. This was introduced by 0.0.9-alpha.4, then incorrectly fixed in 0.0.9-alpha.7 and (hopefully) fixed in **alpha.8**.