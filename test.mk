test.clean:
	rm -rf tests/pulp-sdk
	rm -rf tests/chimera-sdk

test.checkout.pulp-sdk:
	@if [ ! -d "tests/pulp-sdk" ]; then \
		git clone "git@github.com:pulp-platform/pulp-sdk.git" "tests/pulp-sdk"; \
	fi
	cd "tests/pulp-sdk" && \
	git fetch --all && \
	git checkout 9627e586916ea69d58efa69cb7068a40b85e0632

test.checkout.chimera-sdk:
	@if [ ! -d "tests/chimera-sdk" ]; then \
		git clone "git@github.com:pulp-platform/chimera-sdk.git" "tests/chimera-sdk"; \
	fi
	cd "tests/chimera-sdk" && \
	git fetch --all && \
	git checkout b2392f6efcff75c03f4c65eaf3e12104442b22ea

test.checkout: test.checkout.pulp-sdk test.checkout.chimera-sdk

test.build.chimera-sdk:
	cd tests/chimera-sdk && cmake -DTARGET_PLATFORM=chimera-open -DTOOLCHAIN_DIR=$(CHIMERA_LLVM) -B build
	cd tests/chimera-sdk && cmake --build build -j

test.build: test.build.chimera-sdk

test.run:
	plptest --target pulp-open --target chimera --max-timeout 60 run table junit

test: test.checkout test.build test.run

.PHONY: test
