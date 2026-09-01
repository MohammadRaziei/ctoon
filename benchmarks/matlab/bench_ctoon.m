function bench_ctoon(manifestPath)
%BENCH_CTOON  CToon MATLAB benchmark.
%
%   BENCH_CTOON(MANIFESTPATH) runs the same methodology as every other
%   language benchmark in this repo (see benchmarks/README.md), so the
%   numbers line up with the C, C++, Python, and Go results for the same
%   corpus:
%
%     1. Load every file in the corpus manifest into memory (untimed).
%     2. Untimed pre-pass: convert each JSON file to TOON once.
%     3. Timed "JSON -> TOON": repeatedly parse JSON (built-in jsondecode,
%        since the ctoon MATLAB binding does not wrap JSON itself) and
%        re-serialise to TOON with ctoon.dumps.
%     4. Timed "TOON -> JSON": repeatedly parse TOON with ctoon.loads and
%        re-serialise to JSON with the built-in jsonencode.
%     5. Report throughput (MB/s of the bytes actually read by that
%        operation) and documents/sec.
%
%   Must be run with the MATLAB start directory (-sd) set to
%   src/bindings/matlab, so that ctoon.dumps / ctoon.loads resolve —
%   exactly like tests/matlab/test_ctoon.m does.

    repeats = 20;

    fid = fopen(manifestPath, 'r');
    if fid == -1
        error('bench_ctoon:manifest', 'Cannot open manifest: %s', manifestPath);
    end
    lines = textscan(fid, '%s', 'Delimiter', '\n');
    fclose(fid);
    paths = lines{1};

    files = struct('path', {}, 'json', {}, 'toon', {});
    for i = 1:numel(paths)
        p = strtrim(paths{i});
        if isempty(p)
            continue;
        end
        try
            f = fopen(p, 'r');
            if f == -1
                continue;
            end
            raw = fread(f, Inf, '*char')';
            fclose(f);
        catch
            continue;
        end
        files(end+1) = struct('path', p, 'json', raw, 'toon', ''); %#ok<AGROW>
    end

    if isempty(files)
        error('bench_ctoon:corpus', 'Corpus manifest is empty - nothing to benchmark.');
    end

    totalJSONBytes = 0;
    for i = 1:numel(files)
        totalJSONBytes = totalJSONBytes + numel(files(i).json);
    end

    fprintf('CToon MATLAB Benchmark\n');
    fprintf('Corpus: %d files, %.2f MB (JSON)\n\n', numel(files), totalJSONBytes / 1e6);

    % -- Untimed pre-pass --
    totalTOONBytes = 0;
    for i = 1:numel(files)
        try
            val = jsondecode(files(i).json);
            toon = ctoon.dumps(val);
            files(i).toon = toon;
            totalTOONBytes = totalTOONBytes + numel(toon);
        catch
            % skip files that don't round-trip (e.g. scalar-only JSON)
        end
    end

    % -- Phase A: JSON -> TOON --
    tic;
    opsA = 0;
    for r = 1:repeats
        for i = 1:numel(files)
            try
                val = jsondecode(files(i).json);
                ctoon.dumps(val);
                opsA = opsA + 1;
            catch
            end
        end
    end
    tJsonToToon = toc;
    bytesA = totalJSONBytes * repeats;

    % -- Phase B: TOON -> JSON --
    tic;
    opsB = 0;
    for r = 1:repeats
        for i = 1:numel(files)
            if isempty(files(i).toon)
                continue;
            end
            try
                val = ctoon.loads(files(i).toon);
                jsonencode(val);
                opsB = opsB + 1;
            catch
            end
        end
    end
    tToonToJson = toc;
    bytesB = totalTOONBytes * repeats;

    % -- Report --
    fprintf('%-14s %12s %14s %12s\n', 'Operation', 'Throughput', 'Docs/sec', 'Total time');
    fprintf('%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n', ...
        'JSON -> TOON', bytesA / tJsonToToon / 1e6, opsA / tJsonToToon, tJsonToToon, repeats);
    fprintf('%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n', ...
        'TOON -> JSON', bytesB / tToonToJson / 1e6, opsB / tToonToJson, tToonToJson, repeats);
end
