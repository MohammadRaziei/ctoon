%% Main function
function tests = test_ctoon_mex
tests = functiontests(localfunctions);
end

%% File fixtures

function setupOnce(testCase)
here = fileparts(mfilename('fullpath'));
testCase.TestData.DataDir = fullfile(here, '..', 'data');
end

function teardownOnce(~)
end

%% -------------------------------------------------------------------------
%  ctoon_decode — scalar primitives
%% -------------------------------------------------------------------------

function testDecodeNull(testCase)
v = ctoon_decode('null');
verifyEmpty(testCase, v);
verifyClass(testCase, v, 'double');
end

function testDecodeTrue(testCase)
v = ctoon_decode('true');
verifyClass(testCase, v, 'logical');
verifyTrue(testCase, v);
end

function testDecodeFalse(testCase)
v = ctoon_decode('false');
verifyClass(testCase, v, 'logical');
verifyFalse(testCase, v);
end

function testDecodeUint(testCase)
v = ctoon_decode('42');
verifyEqual(testCase, double(v), 42);
end

function testDecodeSint(testCase)
v = ctoon_decode('-7');
verifyEqual(testCase, double(v), -7);
end

function testDecodeReal(testCase)
v = ctoon_decode('3.14');
verifyClass(testCase, v, 'double');
verifyEqual(testCase, v, 3.14, 'AbsTol', 1e-10);
end

function testDecodeString(testCase)
% Bare string (no quotes needed in TOON)
v = ctoon_decode('hello');
verifyEqual(testCase, v, 'hello');
end

%% -------------------------------------------------------------------------
%  ctoon_decode — composite types
%% -------------------------------------------------------------------------

function testDecodeArray(testCase)
% TOON array syntax: [n]: v1,v2,...
v = ctoon_decode('[3]: 1,2,3');
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), 3);
verifyEqual(testCase, double(v{1}), 1);
verifyEqual(testCase, double(v{2}), 2);
verifyEqual(testCase, double(v{3}), 3);
end

function testDecodeObject(testCase)
% TOON object syntax: key: value (newline separated)
toon = sprintf('name: Alice\nage: 30\nactive: true');
v = ctoon_decode(toon);
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.name, 'Alice');
verifyEqual(testCase, double(v.age), 30);
verifyClass(testCase, v.active, 'logical');
verifyTrue(testCase, v.active);
end

function testDecodeNestedObject(testCase)
% Nested object via indentation
toon = sprintf('person:\n  name: Bob\n  age: 25');
v = ctoon_decode(toon);
verifyClass(testCase, v, 'struct');
verifyClass(testCase, v.person, 'struct');
verifyEqual(testCase, v.person.name, 'Bob');
verifyEqual(testCase, double(v.person.age), 25);
end

function testDecodeObjectWithArray(testCase)
toon = sprintf('name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization');
v = ctoon_decode(toon);
verifyEqual(testCase, v.name, 'Alice');
verifyEqual(testCase, double(v.age), 30);
verifyTrue(testCase, v.active);
verifyClass(testCase, v.tags, 'cell');
verifyEqual(testCase, numel(v.tags), 3);
end

%% -------------------------------------------------------------------------
%  ctoon_encode / ctoon_decode round-trip
%% -------------------------------------------------------------------------

function testRoundTripReal(testCase)
% Use non-integer double to avoid uint64 promotion
original = 3.14159;
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'double');
verifyEqual(testCase, v, original, 'AbsTol', 1e-10);
end

function testRoundTripString(testCase)
original = 'world';
v = ctoon_decode(ctoon_encode(original));
verifyEqual(testCase, v, original);
end

function testRoundTripLogicalTrue(testCase)
v = ctoon_decode(ctoon_encode(true));
verifyClass(testCase, v, 'logical');
verifyTrue(testCase, v);
end

function testRoundTripLogicalFalse(testCase)
v = ctoon_decode(ctoon_encode(false));
verifyClass(testCase, v, 'logical');
verifyFalse(testCase, v);
end

function testRoundTripInt64(testCase)
original = int64(-999);
v = ctoon_decode(ctoon_encode(original));
verifyEqual(testCase, double(v), double(original));
end

function testRoundTripUint64(testCase)
original = uint64(2^40);
v = ctoon_decode(ctoon_encode(original));
verifyEqual(testCase, double(v), double(original));
end

function testRoundTripCell(testCase)
original = {1.5, 'abc', false};
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), numel(original));
end

function testRoundTripStruct(testCase)
% Use non-integer doubles to avoid uint64 promotion on round-trip
original.x     = 1.5;
original.label = 'point';
original.flag  = false;
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.x, original.x, 'AbsTol', 1e-10);
verifyEqual(testCase, v.label, original.label);
verifyClass(testCase, v.flag, 'logical');
verifyFalse(testCase, v.flag);
end

%% -------------------------------------------------------------------------
%  ctoon_read / ctoon_write — file I/O
%% -------------------------------------------------------------------------

function testReadSample1(testCase)
sample = fullfile(testCase.TestData.DataDir, 'sample1_user.toon');
assumeTrue(testCase, isfile(sample), 'Test data file not found — skipping.');
v = ctoon_read(sample);
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
ctoon_write(original, tmp);
verifyTrue(testCase, isfile(tmp));
v = ctoon_read(tmp);
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.pi, original.pi, 'AbsTol', 1e-10);
verifyEqual(testCase, v.label, original.label);
verifyClass(testCase, v.ok, 'logical');
verifyTrue(testCase, v.ok);
end

%% -------------------------------------------------------------------------
%  Error handling
%% -------------------------------------------------------------------------

function testDecodeInvalidToon(testCase)
% Empty string produces CTOON_READ_ERROR_EMPTY_CONTENT
verifyError(testCase, @() ctoon_decode(''), 'ctoon:decodeError');
end

function testReadMissingFile(testCase)
verifyError(testCase, @() ctoon_read('/no/such/file.toon'), 'ctoon:readError');
end

function testDecodeNonString(testCase)
verifyError(testCase, @() ctoon_decode(42), 'ctoon:badArg');
end

function testWriteInvalidPath(testCase)
verifyError(testCase, @() ctoon_write(struct('x', 1.5), '/no/such/dir/out.toon'), ...
    'ctoon:writeError');
end

%% -------------------------------------------------------------------------
%  Helpers
%% -------------------------------------------------------------------------

function deleteIfExists(f)
if isfile(f), delete(f); end
end