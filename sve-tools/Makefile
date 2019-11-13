all: merge analyze bundle

merge:
	make -C memtrace_merger

analyze:
	make -C memtrace_analyzer

bundle:
	make -C memtrace_bundle

clean:
	make -C memtrace_merger clean
	make -C memtrace_analyzer clean
	make -C memtrace_bundle clean
