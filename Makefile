LIBDIR= ball fault kani pubsub msgtest clog audit funky iq4app hl7 mongo cpet dicom fhir benchmarker
APPDIR= dicomimporter fhirimporter hl7listener importer flow morph orca callout deidentify router sink squeal validator xlsimporter report loader cpet
ALLDIR=$(LIBDIR) $(APPDIR)

TAGDIR=$(ALLDIR:%=tag-%)
TESTDIR=(ALLDIR:%-test-%)

LIBTST=$(LIBDIR:%=libtest-%)
INSTALLDIRS = $(SUBDIRS:%=install-%)

lib-test: $(LIBTST)
$(LIBTST):
	$(MAKE) -C $(@:libtest-%=%) test-verbose clean

.phony: subdirs $(LIBTST)
.phone: lib-test
