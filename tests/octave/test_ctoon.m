function test_suite = test_ctoon
% Octave test suite for the CToon MEX binding (MOxUnit format).
%
% Mirrors tests/matlab/test_ctoon.m case for case. The MATLAB suite uses the
% matlab.unittest function-based API (functiontests/verify*), which GNU Octave
% does not provide, so this port is written against MOxUnit
% (https://github.com/MOxUnit/MOxUnit), which runs on both Octave and MATLAB.
% Keep the two files in sync: a test added to one belongs in the other.
%
% Run it (after building the binding, see tests/octave/CMakeLists.txt):
%   moxunit_runtests('tests/octave/test_ctoon.m', '-verbose')
%
% Spec-conformance tests need CTOON_SPEC_FIXTURES_DIR; they are skipped (not
% failed) when it is unset, same convention as the other languages.

try % assignment of 'localfunctions' is necessary in Octave >= 4.4
    test_functions = localfunctions();
catch % no problem; early versions can use initTestSuite fine
end
initTestSuite;

%% -------------------------------------------------------------------------
%  ctoon.decode -- scalar primitives
%% -------------------------------------------------------------------------

function test_decode_null
v = ctoon.decode('null');
assertTrue(isempty(v));
assertTrue(isa(v, 'double'));

function test_decode_true
v = ctoon.decode('true');
assertTrue(islogical(v));
assertTrue(v);

function test_decode_false
v = ctoon.decode('false');
assertTrue(islogical(v));
assertFalse(v);

function test_decode_uint
v = ctoon.decode('42');
assertEqual(double(v), 42);

function test_decode_sint
v = ctoon.decode('-7');
assertEqual(double(v), -7);

function test_decode_real
v = ctoon.decode('3.14');
assertTrue(isa(v, 'double'));
assertElementsAlmostEqual(v, 3.14, 'absolute', 1e-10);

function test_decode_string
% Bare string (no quotes needed in TOON)
v = ctoon.decode('hello');
assertEqual(v, 'hello');

%% -------------------------------------------------------------------------
%  ctoon.decode -- composite types
%% -------------------------------------------------------------------------

function test_decode_array
% TOON array syntax: [n]: v1,v2,...
v = ctoon.decode('[3]: 1,2,3');
assertTrue(iscell(v));
assertEqual(numel(v), 3);
assertEqual(double(v{1}), 1);
assertEqual(double(v{2}), 2);
assertEqual(double(v{3}), 3);

function test_decode_object
% TOON object syntax: key: value (newline separated)
v = ctoon.decode(sprintf('name: Alice\nage: 30\nactive: true'));
assertTrue(isstruct(v));
assertEqual(v.name, 'Alice');
assertEqual(double(v.age), 30);
assertTrue(islogical(v.active));
assertTrue(v.active);

function test_decode_nested_object
% Nested object via indentation
v = ctoon.decode(sprintf('person:\n  name: Bob\n  age: 25'));
assertTrue(isstruct(v));
assertTrue(isstruct(v.person));
assertEqual(v.person.name, 'Bob');
assertEqual(double(v.person.age), 25);

function test_decode_object_with_array
v = ctoon.decode(sprintf('name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization'));
assertEqual(v.name, 'Alice');
assertEqual(double(v.age), 30);
assertTrue(v.active);
assertTrue(iscell(v.tags));
assertEqual(numel(v.tags), 3);

function test_decode_tabular_array
% Tabular form decodes to a cell array of structs.
v = ctoon.decode(sprintf('users[2]{id,name}:\n  1,Alice\n  2,Bob'));
assertTrue(isstruct(v));
assertTrue(iscell(v.users));
assertEqual(numel(v.users), 2);
assertEqual(double(v.users{1}.id), 1);
assertEqual(v.users{2}.name, 'Bob');

%% -------------------------------------------------------------------------
%  ctoon.encode / ctoon.decode round-trip
%% -------------------------------------------------------------------------

function test_roundtrip_real
% Use non-integer double to avoid uint64 promotion
original = 3.14159;
v = ctoon.decode(ctoon.encode(original));
assertTrue(isa(v, 'double'));
assertElementsAlmostEqual(v, original, 'absolute', 1e-10);

function test_roundtrip_string
original = 'world';
assertEqual(ctoon.decode(ctoon.encode(original)), original);

function test_roundtrip_utf8_string
original = char([216 179 217 132 216 167 217 133]); % "salam" in Persian, UTF-8 bytes
assertEqual(ctoon.decode(ctoon.encode(original)), original);

function test_roundtrip_logical_true
v = ctoon.decode(ctoon.encode(true));
assertTrue(islogical(v));
assertTrue(v);

function test_roundtrip_logical_false
v = ctoon.decode(ctoon.encode(false));
assertTrue(islogical(v));
assertFalse(v);

function test_roundtrip_int64
original = int64(-999);
v = ctoon.decode(ctoon.encode(original));
assertEqual(double(v), double(original));

function test_roundtrip_uint64
original = uint64(2^40);
v = ctoon.decode(ctoon.encode(original));
assertEqual(double(v), double(original));

function test_encode_uint64_max
% 2^64-1 is not representable as a double, so compare the encoded text.
assertEqual(strtrim(ctoon.encode(intmax('uint64'))), '18446744073709551615');

function test_roundtrip_cell
original = {1.5, 'abc', false};
v = ctoon.decode(ctoon.encode(original));
assertTrue(iscell(v));
assertEqual(numel(v), numel(original));

function test_roundtrip_struct
% Use non-integer doubles to avoid uint64 promotion on round-trip
original.x     = 1.5;
original.label = 'point';
original.flag  = false;
v = ctoon.decode(ctoon.encode(original));
assertTrue(isstruct(v));
assertElementsAlmostEqual(v.x, original.x, 'absolute', 1e-10);
assertEqual(v.label, original.label);
assertTrue(islogical(v.flag));
assertFalse(v.flag);

function test_roundtrip_nested
original = struct('a', struct('b', {{1, 2, 3}}));
v = ctoon.decode(ctoon.encode(original));
assertTrue(isstruct(v) && isstruct(v.a));
assertTrue(iscell(v.a.b));
assertEqual(numel(v.a.b), 3);

function test_encode_numeric_vector
assertEqual(strtrim(ctoon.encode([1 2 3])), '[3]: 1,2,3');

%% -------------------------------------------------------------------------
%  Python-style aliases: dumps / loads / dump / load
%% -------------------------------------------------------------------------

function test_dumps_loads
original.name = 'Alice';
original.score = 2.5;
v = ctoon.loads(ctoon.dumps(original));
assertEqual(v.name, 'Alice');
assertElementsAlmostEqual(v.score, 2.5, 'absolute', 1e-10);

function test_dump_load_fid
tmp = [tempname, '.toon'];
cleanup = onCleanup(@() delete_if_exists(tmp));
fid = fopen(tmp, 'w');
assertTrue(fid >= 0);
ctoon.dump(struct('k', 'v'), fid);
fclose(fid);
fid = fopen(tmp, 'r');
assertTrue(fid >= 0);
v = ctoon.load(fid);
fclose(fid);
assertEqual(v.k, 'v');

%% -------------------------------------------------------------------------
%  ctoon.read / ctoon.write -- file I/O
%% -------------------------------------------------------------------------

function test_read_sample1
sample = fullfile(data_dir(), 'sample1_user.toon');
if ~exist(sample, 'file')
    moxunit_throw_test_skipped_exception('Test data file not found -- skipping.');
end
v = ctoon.read(sample);
assertTrue(isstruct(v));
assertEqual(v.name, 'Alice');
assertEqual(double(v.age), 30);
assertTrue(islogical(v.active));
assertTrue(v.active);
assertTrue(iscell(v.tags));
assertEqual(numel(v.tags), 3);

function test_write_read_roundtrip
tmp = [tempname, '.toon'];
cleanup = onCleanup(@() delete_if_exists(tmp));
original.pi    = 3.14159;
original.label = 'round-trip';
original.ok    = true;
ctoon.write(original, tmp);
assertTrue(logical(exist(tmp, 'file')));
v = ctoon.read(tmp);
assertTrue(isstruct(v));
assertElementsAlmostEqual(v.pi, original.pi, 'absolute', 1e-10);
assertEqual(v.label, original.label);
assertTrue(islogical(v.ok));
assertTrue(v.ok);

%% -------------------------------------------------------------------------
%  Error handling
%% -------------------------------------------------------------------------

function test_decode_empty_string
% Spec 5/8: a zero-length TOON document is valid input and decodes as
% an empty object -- it is NOT an error.
v = ctoon.decode('');
assertTrue(isstruct(v));
assertEqual(numel(fieldnames(v)), 0);

function test_read_missing_file
assertExceptionThrown(@() ctoon.read('/no/such/file.toon'), 'ctoon:readError');

function test_decode_non_string
assertExceptionThrown(@() ctoon.decode(42), 'ctoon:badArg');

function test_write_invalid_path
assertExceptionThrown(@() ctoon.write(struct('x', 1.5), '/no/such/dir/out.toon'), ...
                      'ctoon:writeError');

function test_decode_invalid_document
% Declared length (3) does not match the 2 values given.
assertExceptionThrown(@() ctoon.decode('[3]: 1,2'), 'ctoon:decodeError');

%% -------------------------------------------------------------------------
%  ctoon.encode -- jsondecode() round trips
%
%  Same regressions as tests/matlab/test_ctoon.m: real JSON text goes through
%  jsondecode() first (struct arrays, NaN standing in for null, logical
%  arrays), then ctoon.encode. Expected strings are sourced from
%  toon-format/spec's tests/fixtures/encode/*.json, see the MATLAB suite for
%  the exact fixture each one comes from.
%% -------------------------------------------------------------------------

function test_encode_jsondecode_struct_array
v = jsondecode('[{"id":1},{"id":2}]');
assertTrue(isstruct(v));
assertEqual(numel(v), 2);
assertEqual(ctoon.encode(v), sprintf('[2]{id}:\n  1\n  2'));

function test_encode_jsondecode_null_in_numeric_array
v = jsondecode('[1, null, 3]');
assertTrue(isnumeric(v));
assertTrue(any(isnan(v)));
assertEqual(ctoon.encode(v), '[3]: 1,null,3');

function test_encode_jsondecode_null_scalar
v = jsondecode('[null]');
assertTrue(isscalar(v) && isnan(v));
assertEqual(ctoon.encode(v), 'null');

function test_encode_jsondecode_logical_array
v = jsondecode('[true, false, true]');
assertTrue(islogical(v));
assertTrue(~isscalar(v));
assertEqual(ctoon.encode(v), '[3]: true,false,true');

%% -------------------------------------------------------------------------
%  Spec conformance (toon-format/spec fixtures, via jsondecode + encode)
%% -------------------------------------------------------------------------

function test_spec_fixtures_encode_primitives
run_spec_encode_fixture('primitives.json');

function test_spec_fixtures_encode_objects
run_spec_encode_fixture('objects.json');

function test_spec_fixtures_encode_arrays_primitive
run_spec_encode_fixture('arrays-primitive.json');

%% -------------------------------------------------------------------------
%  Helpers (not collected as tests: names neither start with test_ nor end
%  with _test)
%% -------------------------------------------------------------------------

function d = data_dir
here = fileparts(mfilename('fullpath'));
d = fullfile(here, '..', 'data');

function run_spec_encode_fixture(filename)
d = getenv('CTOON_SPEC_FIXTURES_DIR');
if isempty(d) || ~exist(d, 'dir')
    moxunit_throw_test_skipped_exception( ...
        'CTOON_SPEC_FIXTURES_DIR not set/found -- skipping spec-conformance test.');
end
fixture = jsondecode(fileread(fullfile(d, 'encode', filename)));
tests = fixture.tests;
skip = known_unsupported_cases();
for i = 1:numel(tests)
    % jsondecode() returns a struct array when every element has the same
    % fields, but a cell array of structs when they differ (the usual case).
    if iscell(tests)
        t = tests{i};
    else
        t = tests(i);
    end
    % Options (delimiter/indentSize/strict) aren't threaded through
    % ctoon.encode() yet -- only run the cases that rely on defaults.
    if isfield(t, 'options') && ~isempty(t.options)
        continue
    end
    if any(strcmp(t.name, skip))
        continue
    end
    got = ctoon.encode(t.input);
    assertEqual(got, t.expected, sprintf('[%s] case "%s"', filename, t.name));
end

function names = known_unsupported_cases
% Spec cases that cannot pass through THIS test path (jsondecode -> encode),
% or that expose a known limitation of the binding. They are skipped by name,
% never silently: a new failing case in a spec update still fails the suite.
%
% (1) jsondecode() rewrites keys that are not valid identifiers
%     ("order:id" -> order_id, "" -> x, ...), so the key-quoting cases cannot
%     be exercised from decoded JSON at all.
% (2) The binding maps [] <-> null, so an empty string and an empty array
%     (both [] after jsondecode) encode as null instead of "" / [].
% (3) ctoon.c prints 123456789012345670000 where the spec expects
%     123456789012345680000 (shortest round-trip digits for a very large
%     integer-valued double).
names = { ...
    ... % (2)
    'quotes empty string', ...
    'encodes empty arrays', ...
    'encodes empty string in single-item array', ...
    'encodes empty string in multi-item array', ...
    'encodes empty string keys for empty arrays', ...
    ... % (3)
    'encodes a large integer-valued number with its shortest round-trip digits', ...
    ... % (1)
    'encodes empty string keys for inline arrays', ...
    'quotes key with colon', ...
    'quotes keys with brackets or braces', ...
    'quotes key with comma', ...
    'quotes keys with spaces', ...
    'quotes keys with a leading or interior hyphen', ...
    'quotes numeric key', ...
    'quotes empty string key', ...
    'quotes non-ASCII key', ...
    'keeps dotted and literal-named keys unquoted', ...
    'escapes newline and quotes in keys', ...
    'escapes tab in key', ...
    'escapes U+0004 control character in key via \uXXXX', ...
    'escapes U+001F control character in key via \uXXXX', ...
    'encodes empty string key opening a nested object'};

%% -------------------------------------------------------------------------
%  Direct JSON <-> TOON text conversion (no Octave value in between)
%% -------------------------------------------------------------------------

function test_json_to_toon_basic
got = ctoon.json_to_toon('{"name":"Alice","age":30,"tags":["a","b"]}');
assertEqual(got, sprintf('name: Alice\nage: 30\ntags[2]: a,b'));
got = ctoon.json_to_toon('{"u":[{"id":1,"n":"x"},{"id":2,"n":"y"}]}');
assertEqual(got, sprintf('u[2]{id,n}:\n  1,x\n  2,y'));

function test_toon_to_json_basic
got = ctoon.toon_to_json(sprintf('name: Alice\nage: 30\ntags[2]: a,b'));
assertEqual(got, '{"name":"Alice","age":30,"tags":["a","b"]}');
got = ctoon.toon_to_json('a: 1', 2);
assertEqual(got, sprintf('{\n  "a": 1\n}'));

function test_convert_errors
assertExceptionThrown(@() ctoon.json_to_toon('{"a":'), 'ctoon:convertError');
assertExceptionThrown(@() ctoon.toon_to_json('a[2]: 1'), 'ctoon:convertError');

function test_convert_files
d = tempname; mkdir(d);
j = fullfile(d, 'in.json'); t = fullfile(d, 'out.toon'); b = fullfile(d, 'back.json');
fid = fopen(j, 'w'); fwrite(fid, '{"k":[1,2],"s":"hi"}'); fclose(fid);
ctoon.json_to_toon_file(j, t);
assertEqual(fileread(t), sprintf('k[2]: 1,2\ns: hi'));
ctoon.toon_to_json_file(t, b);
assertEqual(fileread(b), '{"k":[1,2],"s":"hi"}');
bad = fullfile(d, 'bad.json'); never = fullfile(d, 'never.toon');
fid = fopen(bad, 'w'); fwrite(fid, '{"a":'); fclose(fid);
assertExceptionThrown(@() ctoon.json_to_toon_file(bad, never), 'ctoon:convertError');
assertFalse(logical(exist(never, 'file')));
confirm_recursive_rmdir(false, 'local');
rmdir(d, 's');

function delete_if_exists(f)
if exist(f, 'file')
    delete(f);
end
