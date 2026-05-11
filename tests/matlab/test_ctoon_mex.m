%% Main function
function tests = test_ctoon_mex
tests = functiontests(localfunctions);
end

%% File fixtures

function setupOnce(testCase)
% Locate test data directory relative to this test file
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
v = ctoon_decode('2.718');
verifyClass(testCase, v, 'double');
verifyEqual(testCase, v, 2.718, 'AbsTol', 1e-10);
end

function testDecodeString(testCase)
v = ctoon_decode('"hello"');
verifyEqual(testCase, v, 'hello');
end

%% -------------------------------------------------------------------------
%  ctoon_decode — composite types
%% -------------------------------------------------------------------------

function testDecodeArray(testCase)
v = ctoon_decode('[1,2,3]');
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), 3);
end

function testDecodeObject(testCase)
v = ctoon_decode('{name:Alice,active:true}');
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.name, 'Alice');
verifyClass(testCase, v.active, 'logical');
verifyTrue(testCase, v.active);
end

function testDecodeNestedObject(testCase)
v = ctoon_decode('{a:{b:1}}');
verifyClass(testCase, v, 'struct');
verifyClass(testCase, v.a, 'struct');
verifyEqual(testCase, double(v.a.b), 1);
end

%% -------------------------------------------------------------------------
%  ctoon_encode / ctoon_decode round-trip
%% -------------------------------------------------------------------------

function testRoundTripDouble(testCase)
original = 3.14159;
v = ctoon_decode(ctoon_encode(original));
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
original = {42.0, 'abc', false};
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), numel(original));
end

function testRoundTripStruct(testCase)
original.x     = 1.0;
original.label = 'point';
original.flag  = false;
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'struct');
verifyEqual(testCase, v.x, original.x, 'AbsTol', 1e-10);
verifyEqual(testCase, v.label, original.label);
verifyClass(testCase, v.flag, 'logical');
verifyFalse(testCase, v.flag);
end

function testRoundTripDoubleArray(testCase)
original = [1.0, 2.0, 3.0];
v = ctoon_decode(ctoon_encode(original));
verifyClass(testCase, v, 'cell');
verifyEqual(testCase, numel(v), 3);
verifyEqual(testCase, double(v{1}), 1.0, 'AbsTol', 1e-10);
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
verifyError(testCase, @() ctoon_decode('{invalid!!!'), 'ctoon:decodeError');
end

function testReadMissingFile(testCase)
verifyError(testCase, @() ctoon_read('/no/such/file.toon'), 'ctoon:readError');
end

function testDecodeNonString(testCase)
verifyError(testCase, @() ctoon_decode(42), 'ctoon:badArg');
end

function testWriteInvalidPath(testCase)
verifyError(testCase, @() ctoon_write(struct('x',1), '/no/such/dir/out.toon'), ...
    'ctoon:writeError');
end

%% -------------------------------------------------------------------------
%  Helpers
%% -------------------------------------------------------------------------

function deleteIfExists(f)
if isfile(f), delete(f); end
end