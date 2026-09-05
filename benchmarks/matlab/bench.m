function bench(manifestPath, resultsJsonPath)
%BENCH  CToon MATLAB benchmark.
%
%   No competing MATLAB implementation exists, so this is a solo run — but
%   it follows the same methodology, corpus, and JSON output schema as
%   every other language benchmark here.
%
%   Must be run with the MATLAB start directory (-sd) set to
%   src/bindings/matlab of the fetched ctoon checkout, so ctoon.dumps /
%   ctoon.loads resolve.

    repeats = 20;

    fid = fopen(manifestPath, 'r');
    if fid == -1
        error('bench:manifest', 'Cannot open manifest: %s', manifestPath);
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
        error('bench:corpus', 'Corpus manifest is empty - nothing to benchmark.');
    end

    totalJSONBytes = 0;
    for i = 1:numel(files)
        totalJSONBytes = totalJSONBytes + numel(files(i).json);
    end

    fprintf('CToon Benchmarks — MATLAB\n');
    fprintf('Corpus: %d files, %.2f MB (JSON)\n\n', numel(files), totalJSONBytes / 1e6);
    fprintf('%-8s %-14s %12s %14s %9s %10s %12s\n', ...
        'Library', 'Operation', 'Throughput', 'Docs/sec', 'Success', '', 'Total time');

    results = {};

    % -- Untimed pre-pass --
    for i = 1:numel(files)
        try
            val = jsondecode(files(i).json);
            files(i).toon = ctoon.dumps(val);
        catch
        end
    end

    % -- ctoon: json_to_toon --
    tic;
    opsA = 0; bytesA = 0;
    for r = 1:repeats
        for i = 1:numel(files)
            try
                val = jsondecode(files(i).json);
                ctoon.dumps(val);
                opsA = opsA + 1;
                bytesA = bytesA + numel(files(i).json);
            catch
            end
        end
    end
    tA = toc;
    results{end+1} = record('ctoon', 'json_to_toon', bytesA, opsA, tA, numel(files) * repeats, repeats); %#ok<AGROW>

    % -- ctoon: toon_to_json --
    tic;
    opsB = 0; bytesB = 0;
    for r = 1:repeats
        for i = 1:numel(files)
            if isempty(files(i).toon)
                continue;
            end
            try
                val = ctoon.loads(files(i).toon);
                jsonencode(val);
                opsB = opsB + 1;
                bytesB = bytesB + numel(files(i).toon);
            catch
            end
        end
    end
    tB = toc;
    results{end+1} = record('ctoon', 'toon_to_json', bytesB, opsB, tB, numel(files) * repeats, repeats); %#ok<AGROW>

    write_results_json(resultsJsonPath, numel(files), totalJSONBytes, results);
end

function r = record(library, operation, bytes, ops, seconds, attempted, repeats)
    if ops > 0
        throughput = bytes / seconds / 1e6;
    else
        throughput = 0;
    end
    successRate = ops / attempted;
    fprintf('%-8s %-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n', ...
        library, operation, throughput, ops / seconds, successRate * 100, seconds, repeats);
    r = struct('library', library, 'operation', operation, ...
        'throughput_mb_s', throughput, 'docs_per_sec', ops / seconds, ...
        'success_rate', successRate, 'total_time_s', seconds);
end

function write_results_json(path, nFiles, totalBytes, results)
    out = struct('language', 'matlab', ...
        'corpus', struct('files', nFiles, 'bytes', totalBytes), ...
        'results', {results});
    fid = fopen(path, 'w');
    if fid == -1
        fprintf('warning: could not write %s\n', path);
        return;
    end
    fprintf(fid, '%s', jsonencode(out));
    fclose(fid);
    fprintf('\nResults written to %s\n', path);
end
