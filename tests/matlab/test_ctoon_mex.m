%% test_ctoon_mex.m
% Unit tests for the CToon MATLAB binding.
%
% Tests the public API:
%   ctoon_encode, ctoon_decode, ctoon_read, ctoon_write
%
% Usage — interactive:
%   addpath('<build>/src/bindings/matlab');
%   addpath('<repo>/tests/matlab');
%   test_ctoon_mex
%
% Usage — non-interactive (CI / cmake -P):
%   matlab -batch "addpath('...'); addpath('...'); test_ctoon_mex"
%
% Exits with a MATLAB error (non-zero exit code) on any failure.

fprintf('=== CToon MATLAB binding test suite ===\n\n');

TEST_DATA_DIR = fullfile(fileparts(mfilename('fullpath')), '..', 'data');

passed = 0;
failed = 0;

% -------------------------------------------------------------------------
% Helpers
% -------------------------------------------------------------------------

function check(cond, name)
    if cond
        fprintf('  [PASS] %s\n', name);
    else
        error('ctoon:testFailed', '[FAIL] %s', name);
    end
end

function check_eq(a, b, name)
    check(isequal(a, b), name);
end

% =========================================================================
% 1. ctoon_decode — scalar primitives
% =========================================================================
fprintf('--- ctoon_decode: scalars ---\n');
try
    check(isempty(ctoon_decode('null')) && isa(ctoon_decode('null'),'double'), ...
          'null  → []');
    passed = passed + 1;
catch e; fprintf('  [FAIL] null: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('true');
    check(islogical(v) && v == true, 'true  → logical 1');
    passed = passed + 1;
catch e; fprintf('  [FAIL] true: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('false');
    check(islogical(v) && v == false, 'false → logical 0');
    passed = passed + 1;
catch e; fprintf('  [FAIL] false: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('42');
    check(isnumeric(v) && double(v) == 42, 'uint 42 → numeric');
    passed = passed + 1;
catch e; fprintf('  [FAIL] uint42: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('-7');
    check(isnumeric(v) && double(v) == -7, 'sint -7 → numeric');
    passed = passed + 1;
catch e; fprintf('  [FAIL] sint-7: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('2.718');
    check(isa(v,'double') && abs(v-2.718)<1e-10, 'real 2.718 → double');
    passed = passed + 1;
catch e; fprintf('  [FAIL] real: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('"hello"');
    check_eq(v, 'hello', 'string "hello" → char');
    passed = passed + 1;
catch e; fprintf('  [FAIL] string: %s\n', e.message); failed=failed+1; end

% =========================================================================
% 2. ctoon_decode — composite types
% =========================================================================
fprintf('\n--- ctoon_decode: composite ---\n');
try
    v = ctoon_decode('[1,2,3]');
    check(iscell(v) && numel(v)==3, 'array → cell(3)');
    passed = passed + 1;
catch e; fprintf('  [FAIL] array: %s\n', e.message); failed=failed+1; end

try
    v = ctoon_decode('{name:Alice,active:true}');
    check(isstruct(v),              'object → struct');
    check_eq(v.name,  'Alice',      'object.name');
    check(islogical(v.active) && v.active, 'object.active');
    passed = passed + 3;
catch e; fprintf('  [FAIL] object: %s\n', e.message); failed=failed+1; end

% =========================================================================
% 3. ctoon_encode / ctoon_decode round-trip
% =========================================================================
fprintf('\n--- ctoon_encode / ctoon_decode round-trip ---\n');

function roundtrip(val, label, passed, failed)
    try
        s = ctoon_encode(val);
        v = ctoon_decode(s);
        if iscell(val)
            ok = iscell(v) && numel(v)==numel(val);
        elseif isstruct(val)
            ok = isstruct(v) && ...
                 all(ismember(fieldnames(val), fieldnames(v)));
        elseif ischar(val)
            ok = isequal(v, val);
        elseif islogical(val)
            ok = islogical(v) && v == val;
        else
            ok = abs(double(v) - double(val)) < 1e-10;
        end
        if ok
            fprintf('  [PASS] encode/decode: %s\n', label);
            passed = passed + 1;
        else
            fprintf('  [FAIL] encode/decode: %s\n', label);
            failed = failed + 1;
        end
    catch e
        fprintf('  [FAIL] encode/decode %s: %s\n', label, e.message);
        failed = failed + 1;
    end
end

roundtrip(3.14159,         'double',  passed, failed);
roundtrip('world',         'string',  passed, failed);
roundtrip(true,            'logical', passed, failed);
roundtrip(int64(-999),     'int64',   passed, failed);
roundtrip(uint64(2^40),    'uint64',  passed, failed);
roundtrip({42.0,'a',false},'cell',    passed, failed);

st.x     = 1.0;
st.label = 'point';
st.flag  = false;
roundtrip(st, 'struct', passed, failed);

% =========================================================================
% 4. ctoon_read / ctoon_write — file I/O
% =========================================================================
fprintf('\n--- ctoon_read / ctoon_write ---\n');

sample1 = fullfile(TEST_DATA_DIR, 'sample1_user.toon');
if exist(sample1, 'file')
    try
        v = ctoon_read(sample1);
        check(isstruct(v),                       'read: → struct');
        check_eq(v.name, 'Alice',                'read: name = Alice');
        check(double(v.age) == 30,               'read: age  = 30');
        check(islogical(v.active) && v.active,   'read: active = true');
        check(iscell(v.tags) && numel(v.tags)==3,'read: tags has 3 items');
        passed = passed + 5;
    catch e; fprintf('  [FAIL] read sample1: %s\n',e.message); failed=failed+1; end
else
    fprintf('  [SKIP] read — data file not found: %s\n', sample1);
end

tmp = [tempname, '.toon'];
try
    orig.pi    = 3.14159;
    orig.label = 'round-trip';
    orig.ok    = true;
    ctoon_write(orig, tmp);
    check(exist(tmp,'file')==2, 'write: file created');
    v = ctoon_read(tmp);
    check(isstruct(v),                     'write/read: struct');
    check(abs(v.pi - 3.14159) < 1e-10,    'write/read: pi');
    check_eq(v.label, 'round-trip',        'write/read: label');
    passed = passed + 4;
catch e; fprintf('  [FAIL] write/read: %s\n',e.message); failed=failed+1; end
if exist(tmp,'file'), delete(tmp); end

% =========================================================================
% 5. Error handling
% =========================================================================
fprintf('\n--- error handling ---\n');
try
    ctoon_decode('{invalid!!!');
    fprintf('  [FAIL] decode invalid: no error thrown\n'); failed=failed+1;
catch
    fprintf('  [PASS] decode invalid TOON → error\n'); passed=passed+1;
end

try
    ctoon_read('/no/such/file.toon');
    fprintf('  [FAIL] read missing: no error thrown\n'); failed=failed+1;
catch
    fprintf('  [PASS] read missing file → error\n'); passed=passed+1;
end


try
    ctoon_decode(42);       % non-string input
    fprintf('  [FAIL] decode non-string: no error thrown\n'); failed=failed+1;
catch
    fprintf('  [PASS] decode non-string input → error\n'); passed=passed+1;
end

% =========================================================================
% Summary
% =========================================================================
fprintf('\n=== Results: %d passed, %d failed ===\n', passed, failed);
if failed > 0
    error('ctoon:testFailed', '%d test(s) failed.', failed);
end
fprintf('All tests passed.\n');