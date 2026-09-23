%% Main function
function tests = test_ctoon
tests = functiontests(localfunctions);
end

%% File fixtures

function setupOnce(testCase)
here = fileparts(mfilename('fullpath'));
testCase.TestData.DataDir = fullfile(here, '..', 'data');
% Set centrally in tests/CMakeLists.txt (FetchContent of toon-format/spec)
% and passed down as an env var, same as tests/python/conftest.py does.
% Empty when the fetch didn't happen (e.g. no network at configure time,
% or running this file directly outside the cmake/buildtool harness) --
% every test that uses it must skip via assumeTrue(...), not fail.
testCase.TestData.SpecFixturesDir = getenv('CTOON_SPEC_FIXTURES_DIR');
end

function teardownOnce(~)
end

%% -------------------------------------------------------------------------
%  ctoon.decode — scalar primitives
%% -------------------------------------------------------------------------

function testDecodeNull(testCase)
v = ctoon.decode('null');
verifyEmpty(testCase, v);
verifyClass(testCase, v, 'double');
end

function testDecodeTrue(testCase)
v = ctoon.decode('true');
verifyClass(testCase, v, 'logical');
verifyTrue(testCase, v);
end

function testDecodeFalse(testCase)
v = ctoon.decode('false');
verifyClass(testCase, v, 'logical');
verifyFalse(testCase, v);
end

function testDecodeUint(testCase)
v = ctoon.decode('42');
verifyEqual(testCase, double(v), 42);
end

function testDecodeSint(testCase)
v = ctoon.decode('-7');
verifyEqual(testCase, double(v), -7);
end

function testDecodeReal(testCase)
v = ctoon.decode('3.14');
verifyClass(testCase, v, 'double');
verifyEqual(testCase, v, 3.14, 'AbsTol', 1e-10);
end

function testDecodeString(testCase)
% Bare string (no quotes needed in TOON)
v = ctoon.decode('hello');
verifyEqual(testCase, v, 'hello');
end

%% -------------------------------------------------------------------------
%  ctoon.decode — composite types
%% -------------------------------------------------------------------------

function testDecodeArray(testCase)
% TOON array syntax: [n]: v1,v2,...
v = ctoon.decode('[3]: 1,2,3');
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), 3);
verifyEqual(testCase, double(v{1}), 1);
verifyEqual(testCase, double(v{2}), 2);
verifyEqual(testCase, double(v{3}), 3);
end

function testDecodeObject(testCase)
% TOON object syntax: key: value (newline separated)
toon = sprintf('name: Alice\nage: 30\nactive: true');
v = ctoon.decode(toon);
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.name, 'Alice');
verifyEqual(testCase, double(v.age), 30);
verifyClass(testCase, v.active, 'logical');
verifyTrue(testCase, v.active);
end

function testDecodeNestedObject(testCase)
% Nested object via indentation
toon = sprintf('person:\n  name: Bob\n  age: 25');
v = ctoon.decode(toon);
verifyClass(testCase, v, 'struct');
verifyClass(testCase, v.person, 'struct');
verifyEqual(testCase, v.person.name, 'Bob');
verifyEqual(testCase, double(v.person.age), 25);
end

function testDecodeObjectWithArray(testCase)
toon = sprintf('name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization');
v = ctoon.decode(toon);
verifyEqual(testCase, v.name, 'Alice');
verifyEqual(testCase, double(v.age), 30);
verifyTrue(testCase, v.active);
verifyClass(testCase, v.tags, 'cell');
verifyEqual(testCase, numel(v.tags), 3);
end

%% -------------------------------------------------------------------------
%  ctoon.encode / ctoon.decode round-trip
%% -------------------------------------------------------------------------

function testRoundTripReal(testCase)
% Use non-integer double to avoid uint64 promotion
original = 3.14159;
v = ctoon.decode(ctoon.encode(original));
verifyClass(testCase, v, 'double');
verifyEqual(testCase, v, original, 'AbsTol', 1e-10);
end

function testRoundTripString(testCase)
original = 'world';
v = ctoon.decode(ctoon.encode(original));
verifyEqual(testCase, v, original);
end

function testRoundTripLogicalTrue(testCase)
v = ctoon.decode(ctoon.encode(true));
verifyClass(testCase, v, 'logical');
verifyTrue(testCase, v);
end

function testRoundTripLogicalFalse(testCase)
v = ctoon.decode(ctoon.encode(false));
verifyClass(testCase, v, 'logical');
verifyFalse(testCase, v);
end

function testRoundTripInt64(testCase)
original = int64(-999);
v = ctoon.decode(ctoon.encode(original));
verifyEqual(testCase, double(v), double(original));
end

function testRoundTripUint64(testCase)
original = uint64(2^40);
v = ctoon.decode(ctoon.encode(original));
verifyEqual(testCase, double(v), double(original));
end

function testRoundTripCell(testCase)
original = {1.5, 'abc', false};
v = ctoon.decode(ctoon.encode(original));
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), numel(original));
end

function testRoundTripStruct(testCase)
% Use non-integer doubles to avoid uint64 promotion on round-trip
original.x     = 1.5;
original.label = 'point';
original.flag  = false;
v = ctoon.decode(ctoon.encode(original));
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.x, original.x, 'AbsTol', 1e-10);
verifyEqual(testCase, v.label, original.label);
verifyClass(testCase, v.flag, 'logical');
verifyFalse(testCase, v.flag);
end

%% -------------------------------------------------------------------------
%  ctoon.read / ctoon.write — file I/O
%% -------------------------------------------------------------------------

function testReadSample1(testCase)
sample = fullfile(testCase.TestData.DataDir, 'sample1_user.toon');
assumeTrue(testCase, isfile(sample), 'Test data file not found — skipping.');
v = ctoon.read(sample);
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.name, 'Alice');
verifyEqual(testCase, double(v.age), 30);
verifyClass(testCase, v.active, 'logical');
verifyTrue(testCase, v.active);
verifyClass(testCase, v.tags, 'cell');
verifyEqual(testCase, numel(v.tags), 3);
end

function testWriteReadRoundTrip(testCase)
tmp = [tempname, '.toon'];
testCase.addTeardown(@() deleteIfExists(tmp));
original.pi    = 3.14159;
original.label = 'round-trip';
original.ok    = true;
ctoon.write(original, tmp);
verifyTrue(testCase, isfile(tmp));
v = ctoon.read(tmp);
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.pi, original.pi, 'AbsTol', 1e-10);
verifyEqual(testCase, v.label, original.label);
verifyClass(testCase, v.ok, 'logical');
verifyTrue(testCase, v.ok);
end

%% -------------------------------------------------------------------------
%  Error handling
%% -------------------------------------------------------------------------

function testDecodeEmptyString(testCase)
% Spec §5/§8: a zero-length TOON document is valid input and decodes as
% an empty object — it is NOT an error.
v = ctoon.decode('');
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, numel(fieldnames(v)), 0);
end

function testReadMissingFile(testCase)
verifyError(testCase, @() ctoon.read('/no/such/file.toon'), 'ctoon:readError');
end

function testDecodeNonString(testCase)
verifyError(testCase, @() ctoon.decode(42), 'ctoon:badArg');
end

function testWriteInvalidPath(testCase)
verifyError(testCase, @() ctoon.write(struct('x', 1.5), '/no/such/dir/out.toon'), ...
    'ctoon:writeError');
end

%% -------------------------------------------------------------------------
%  ctoon.encode — MATLAB-native jsondecode() round trips
%
%  Every test above feeds ctoon.encode/decode hand-built MATLAB literals
%  (struct(...), {...}, scalars) directly -- none of them ever go through
%  jsondecode() first. That untested path (real JSON text -> jsondecode()
%  -> whatever MATLAB type it produces -> ctoon.encode) is exactly where
%  the struct-array / null-as-NaN / logical-array bugs above were hiding;
%  the hand-built literals never exercised jsondecode()'s own type
%  choices (a struct ARRAY for a JSON array of objects, NaN standing in
%  for a JSON null inside a numeric context, etc). Expected TOON output
%  below was captured from the real C CLI (ctoon.c's own writer), not
%  hand-typed, so these assert against ground truth, not intuition.
%% -------------------------------------------------------------------------

function testEncodeJsondecodeStructArray(testCase)
% Regression: mx_to_mut() used to hardcode struct element index 0, so a
% jsondecode()'d top-level JSON array of objects (-> a non-scalar MATLAB
% struct array) silently collapsed to just its first element.
v = jsondecode('[{"a":1,"b":2},{"a":3,"b":4}]');
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, numel(v), 2); % sanity: jsondecode did make a struct array
out = ctoon.encode(v);
verifyEqual(testCase, out, sprintf('[2]{a,b}:\n  1,2\n  3,4'));
end

function testEncodeJsondecodeNullInNumericArray(testCase)
% Regression: jsondecode() substitutes NaN for a JSON `null` inside an
% otherwise-numeric array ([1, null, 3] -> [1 NaN 3]). mx_to_mut() used
% to hand that NaN straight to ctoon_mut_real(), and ctoon_write_num()
% correctly refuses to write a non-finite double -> the whole encode
% failed with "ctoon_mut_write() failed." for any fixture containing a
% null inside a numeric array (this is how it was actually found: see
% draft7/items.json's "allows null elements" case in the JSON-Schema-
% Test-Suite corpus used by benchmarks/). Must round-trip back to null.
v = jsondecode('[1, null, 3]');
verifyTrue(testCase, isnumeric(v));
verifyTrue(testCase, any(isnan(v))); % sanity: this IS the NaN-substitution case
out = ctoon.encode(v);
verifyEqual(testCase, out, '[3]: 1,null,3');
end

function testEncodeJsondecodeNullScalar(testCase)
% Same bug, scalar form: {"data": [null]} decodes "data" to a bare NaN
% scalar (not a 1-element array) -- this was draft7/items.json exactly.
v = jsondecode('[null]');
verifyTrue(testCase, isscalar(v) && isnan(v));
out = ctoon.encode(v);
verifyEqual(testCase, out, 'null');
end

function testEncodeJsondecodeLogicalArray(testCase)
% Regression: only scalar logical was handled; a non-scalar logical
% array (jsondecode() of a JSON array of booleans) fell through to the
% "Unsupported MATLAB type 'logical'" fallback and silently became null.
v = jsondecode('[true, false, true]');
verifyClass(testCase, v, 'logical');
verifyTrue(testCase, ~isscalar(v));
out = ctoon.encode(v);
verifyEqual(testCase, out, '[3]: true,false,true');
end

%% -------------------------------------------------------------------------
%  Spec conformance (toon-format/spec fixtures, via jsondecode + encode)
%
%  Mirrors tests/cpp/test_spec_conformance.cpp and
%  tests/python/test_spec_conformance.py, but through jsondecode() rather
%  than ctoon's own JSON reader -- the same MATLAB-native path the four
%  regression tests above target. Skips (not fails) when
%  CTOON_SPEC_FIXTURES_DIR isn't set, same convention as the rest of the
%  suite (see setupOnce above and tests/python/conftest.py).
%% -------------------------------------------------------------------------

function testSpecFixturesEncodePrimitives(testCase)
runSpecEncodeFixture(testCase, 'primitives.json');
end

function testSpecFixturesEncodeObjects(testCase)
runSpecEncodeFixture(testCase, 'objects.json');
end

function testSpecFixturesEncodeArraysPrimitive(testCase)
runSpecEncodeFixture(testCase, 'arrays-primitive.json');
end

function runSpecEncodeFixture(testCase, filename)
d = testCase.TestData.SpecFixturesDir;
assumeTrue(testCase, ~isempty(d) && isfolder(d), ...
    'CTOON_SPEC_FIXTURES_DIR not set/found -- skipping spec-conformance test.');
raw = fileread(fullfile(d, 'encode', filename));
fixture = jsondecode(raw);
for i = 1:numel(fixture.tests)
    t = fixture.tests(i);
    % Options (delimiter/indentSize/strict) aren't threaded through
    % ctoon.encode() yet -- only run the cases that rely on defaults,
    % same restriction the smoke-test scope here is meant to have.
    if isfield(t, 'options') && ~isempty(t.options)
        continue
    end
    got = ctoon.encode(t.input);
    verifyEqual(testCase, got, t.expected, ...
        sprintf('[%s] case "%s"', filename, t.name));
end
end

%% -------------------------------------------------------------------------
%  Helpers
%% -------------------------------------------------------------------------

function deleteIfExists(f)
if isfile(f), delete(f); end
end