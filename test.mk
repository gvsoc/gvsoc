test.checkout:
	@if [ ! -d "tests/pulp-sdk" ]; then \
		git clone "git@github.com:pulp-platform/pulp-sdk.git" "tests/pulp-sdk"; \
	fi
	cd "tests/pulp-sdk" && \
	git fetch --all && \
	git checkout 9627e586916ea69d58efa69cb7068a40b85e0632

test.run:
	plptest --target pulp-open --max-timeout 60 run table junit

test: test.checkout test.run

.PHONY: test
