# TODO LIST

## Security

* Reassess Jeroen's comment:
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-494040238
> 5. If the security_api just becomes part of ddsc, and it should in my opinion, then I'd prefer you propagate the naming scheme as introduced in ddsrt etc and name the header files e.g. dds/ddssec/auth.h or something instead of dds/security/dds_security_api_authentication.h.

* Reassess Jeroen's comment:
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-494040238
> I've spent a great deal of time stripping out all the various different error codes and make it so that we simply use DDS_RETCODE_ constants everywhere. This pull request reintroduces separate error codes and that's something I really don't approve of. The security error codes start at an offset of 100 and should nicely integrate with the other codes in dds/ddsrt/retcode.h. The messages should be retrievable using dds_strretcode if you ask me.


* reassess Erik's comment
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-490718462
> GuidPrefix & BuiltinTopicKey change

* reassess erik's comment
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-490718462
> ddsrt_strchrs

* Reassess Jeroen's comment:
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-494040238
> If the security_api just becomes part of ddsc, and it should in my opinion, then I'd prefer you propagate the naming scheme as introduced in ddsrt etc and name the header files e.g. dds/ddssec/auth.h or something instead of dds/security/dds_security_api_authentication.h.

* Reassess Jeroen's comment:
https://github.com/eclipse-cyclonedds/cyclonedds/pull/177#issuecomment-494040238
> If the security_api just becomes part of ddsc, and it should in my opinion, then I'd prefer you propagate the naming scheme as introduced in ddsrt etc and name the header files e.g. dds/ddssec/auth.h or something instead of dds/security/dds_security_api_authentication.h.




