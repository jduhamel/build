## -*- mode: Makefile; fill-column: 75; comment-column: 50; -*-

PACKAGE  = $(shell  git remote show origin -n | grep Fetch.URL | sed 's/.*@//;s/.git$$//;s_:_/_' )
REPO = $(shell echo $(PACKAGE) | sed 's_^github.com/__' )
SNAME= $(shell echo $(PACKAGE) | sed 's_.*/__' )
DATE    ?= $(shell date +%FT%T%z)
VERSION ?= $(shell git describe --tags --always --dirty --match=v* 2> /dev/null || \
			cat $(CURDIR)/.version 2> /dev/null || echo v0)
DOCKERREPO=foo.bar
BIN      = $(GOPATH)/bin
BASE     = $(GOPATH)/src/$(PACKAGE)
COMMIT	 = $(shell git rev-parse --short HEAD)

PKGS     = $(or $(PKG),$(shell cd $(BASE) && env GOPATH=$(GOPATH) $(GO) list ./... | grep -v "^$(PACKAGE)/vendor/"))
TESTPKGS = $(shell $(GO) list -f '{{ if .TestGoFiles }}{{ .ImportPath }}{{ end }}' $(PKGS))
LDFLAG 	 = "-s -w -X github.com/iq4health/iq4app/standalone.version=$(VERSION) \
	-X github.com/iq4health/iq4app/standalone.builddate=$(DATE) \
	-X github.com/iq4health/iq4app/standalone.commit=$(COMMIT)"
LINUX   = CGO_ENABLED=0 GOOS=linux
DARWIN   = CGO_ENABLED=0 GOOS=darwin
GO      = go
GODOC   = godoc
GOFMT   = gofmt
GLIDE   = glide
TIMEOUT = 15

V = 0
Q = $(if $(filter 1,$V),,@)
M = $(shell printf "\033[34;1m▶\033[0m")

.PHONY: all
all: fmt lint | $(BASE) ; $(info $(M) building executable...) @ ## Build program binary

roll-tag:
	git checkout develop
	cp glide.yaml glide.yaml.develop
	git checkout -b release/$(RTAG)
	sed -i '' "s/develop/$(RTAG)/g" glide.yaml
	git add glide.yaml
	git commit -m "Updated glide.yaml for $(RTAG)"
	git checkout master
	git merge release/$(RTAG)
	git tag -a $(RTAG) -m "$(RTAG) released."
	git checkout develop
	git merge --no-ff master
	mv glide.yaml.develop glide.yaml
	git add glide.yaml
	git commit -m 'Changed glide.yaml to develop version'
	git branch -d release/$(RTAG)
	git push origin develop
	git push origin master
	git push --tags

docker-build-tag:
	git fetch origin --tags
	git checkout tags/$(TAG)
	make docker-build
	docker tag $(REPO):$(COMMIT) $(DOCKERREPO)/$(SNAME):$(TAG)	
	docker push  $(DOCKERREPO)/$(SNAME):$(TAG)	

docker-build-dev:
	git checkout develop
	make docker-build
	docker tag $(REPO):$(COMMIT) $(DOCKERREPO)/$(SNAME):develop
	docker push  $(DOCKERREPO)/$(SNAME):develop

docker-build: build-linux ; $(info $(M) creating docker image... ) @ ## Build docker image
	$Q cd $(BASE) && docker build --squash $(if $(filter 1,$V),,--quiet) -t $(REPO):$(COMMIT) . &>/dev/null
	$(info $(M) successfully build image tagged as $(REPO):$(COMMIT) ) @ ## Build image

# Tools

GOLINT = $(BIN)/golint
$(BIN)/golint: | $(BASE) ; $(info $(M) building golint...)
	$Q go get github.com/golang/lint/golint

GOCOVMERGE = $(BIN)/gocovmerge
$(BIN)/gocovmerge: | $(BASE) ; $(info $(M) building gocovmerge...)
	$Q go get github.com/wadey/gocovmerge

GOCOV = $(BIN)/gocov
$(BIN)/gocov: | $(BASE) ; $(info $(M) building gocov...)
	$Q go get github.com/axw/gocov/...

GOCOVXML = $(BIN)/gocov-xml
$(BIN)/gocov-xml: | $(BASE) ; $(info $(M) building gocov-xml...)
	$Q go get github.com/AlekSi/gocov-xml

GO2XUNIT = $(BIN)/go2xunit
$(BIN)/go2xunit: | $(BASE) ; $(info $(M) building go2xunit...)
	$Q go get github.com/tebeka/go2xunit

# Tests

TEST_TARGETS := test-default test-bench test-short test-verbose test-race
.PHONY: $(TEST_TARGETS) test-xml check test tests
test-bench:   ARGS=-run=__absolutelynothing__ -bench=. ## Run benchmarks
test-short:   ARGS=-short        ## Run only short tests
test-verbose: ARGS=-v            ## Run tests in verbose mode with coverage reporting
test-race:    ARGS=-race         ## Run tests with race detector
$(TEST_TARGETS): NAME=$(MAKECMDGOALS:test-%=%)
$(TEST_TARGETS): test

check test tests: fmt  | $(BASE) ; $(info $(M) running $(NAME:%=% )tests...) @ ## Run tests
	$Q $(GO) test $(ARGS) $(TESTPKGS)

test-xml: fmt lint | $(BASE) $(GO2XUNIT) ; $(info $(M) running $(NAME:%=% )tests...) @ ## Run tests with xUnit output
	$Q cd $(BASE) && 2>&1 $(GO) test -timeout 20s -v $(TESTPKGS) | tee test/tests.output
	$(GO2XUNIT) -fail -input test/tests.output -output test/tests.xml

COVERAGE_MODE = atomic
COVERAGE_PROFILE = $(COVERAGE_DIR)/profile.out
COVERAGE_XML = $(COVERAGE_DIR)/coverage.xml
COVERAGE_HTML = $(COVERAGE_DIR)/index.html
.PHONY: test-coverage test-coverage-tools
test-coverage-tools: | $(GOCOVMERGE) $(GOCOV) $(GOCOVXML)
test-coverage: COVERAGE_DIR := $(CURDIR)/test/coverage.$(shell date -u +"%Y-%m-%dT%H:%M:%SZ")
test-coverage: fmt lint test-coverage-tools | $(BASE) ; $(info $(M) running coverage tests...) @ ## Run coverage tests
	$Q mkdir -p $(COVERAGE_DIR)/coverage
	$Q cd $(BASE) && for pkg in $(TESTPKGS); do \
		$(GO) test \
			-coverpkg=$$($(GO) list -f '{{ join .Deps "\n" }}' $$pkg | \
					grep '^$(PACKAGE)/' | grep -v '^$(PACKAGE)/vendor/' | \
					tr '\n' ',')$$pkg \
			-covermode=$(COVERAGE_MODE) \
			-coverprofile="$(COVERAGE_DIR)/coverage/`echo $$pkg | tr "/" "-"`.cover" $$pkg ;\
	 done
	$Q $(GOCOVMERGE) $(COVERAGE_DIR)/coverage/*.cover > $(COVERAGE_PROFILE)
	$Q $(GO) tool cover -html=$(COVERAGE_PROFILE) -o $(COVERAGE_HTML)
	$Q $(GOCOV) convert $(COVERAGE_PROFILE) | $(GOCOVXML) > $(COVERAGE_XML)

.PHONY: lint
lint: | $(BASE) $(GOLINT) ; $(info $(M) running golint...) @ ## Run golint
	$Q cd $(BASE) && ret=0 && for pkg in $(PKGS); do \
		test -z "$$($(GOLINT) $$pkg | tee /dev/stderr)" || ret=1 ; \
	 done ; exit $$ret

.PHONY: fmt
fmt: ; $(info $(M) running gofmt...) @ ## Run gofmt on all source files
	@ret=0 && for d in $$($(GO) list -f '{{.Dir}}' ./... | grep -v /vendor/); do \
		$(GOFMT) -l -w $$d/*.go || ret=$$? ; \
	 done ; exit $$ret

# Dependency management

# Misc

.PHONY: help
help:
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-15s\033[0m %s\n", $$1, $$2}'

.PHONY: version
version:
	@echo $(VERSION)


# Local Variables:
# mode: makefile
# End:
