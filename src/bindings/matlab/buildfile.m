function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon MATLAB MEX binding.
%
%   Usage:
%       buildtool                  % default: mex
%       buildtool mex              % compile ctoon_mex
%       buildtool test             % mex + run tests + HTML coverage
%       buildtool coverage         % mex + run tests + HTML + lcov + Cobertura XML
%       buildtool clean            % remove BUILD_DIR and BUILD_COVERAGE_DIR
%       buildtool -tasks           % list tasks
%
%   Configurable variables (set before buildtool or passed via -batch):
%
%       BUILD_DIR          output directory for MEX binary and .m wrappers
%                          (default: directory of this buildfile)
%       BUILD_COVERAGE_DIR coverage report output directory
%                          (default: BUILD_DIR/coverage)
%
%   CMake invocation pattern:
%
%       matlab -sd src/bindings/matlab -batch \
%         "BUILD_DIR='${CMAKE_CURRENT_BINARY_DIR}'; \
%          BUILD_COVERAGE_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'; \
%          buildtool mex"
%
%   Coverage output layout (in BUILD_COVERAGE_DIR):
%       coverage.lcov        lcov tracefile
%       html/                HTML report directory  (index.html inside)
%       cobertura.xml        Cobertura XML  (CI / SonarQube)
%
%   Requirements:
%       MATLAB R2022b+  (Build Tool)
%       MATLAB R2024a+  (addCodeCoverage)
%       R2014b+         for mex compilation via ctoon_build
%
%   See also: buildtool, ctoon_build, ctoon_install,
%             matlab.buildtool.tasks.TestTask.

import matlab.buildtool.tasks.TestTask

% -------------------------------------------------------------------------
% Resolve paths
% -------------------------------------------------------------------------
here    = fileparts(mfilename('fullpath'));
testDir = fullfile(here, '..', '..', '..', 'tests', 'matlab');

buildDir = resolve_ws('BUILD_DIR', here);
covDir   = resolve_ws('BUILD_COVERAGE_DIR', fullfile(buildDir, 'coverage'));

% Coverage output files
covHtmlDir  = fullfile(covDir, 'html');   % → html/index.html
covXmlFile  = fullfile(covDir, 'cobertura.xml');
covLcovFile = fullfile(covDir, 'coverage.lcov');

% -------------------------------------------------------------------------
% Build plan
% -------------------------------------------------------------------------
plan = buildplan(localfunctions);

% "test" — run tests + HTML report only (fast, no lcov)
plan("test") = TestTask(testDir, SourceFiles=buildDir) ...
    .addCodeCoverage(fullfile(covHtmlDir, 'index.html'));

% "coverage" — run tests + HTML + Cobertura XML; lcov is written by
%              coverageTask (local function) which depends on "test"
plan("coverage").Dependencies = "test";

plan("test").Dependencies     = "mex";
plan("clean").Dependencies    = {};

plan.DefaultTasks = "mex";

end % buildfile


% =========================================================================
% Local task functions
% =========================================================================

function mexTask(~)
%MEX  Compile ctoon_mex via ctoon_build.
%
%   Delegates entirely to ctoon_build which reads BUILD_DIR,
%   MEX_SOURCES_PATH and MEX_INCLUDE_DIR from the base workspace.

here = fileparts(mfilename('fullpath'));
run(fullfile(here, 'ctoon_build.m'));

end % mexTask


function coverageTask(context)
%COVERAGE  Produce Cobertura XML and lcov tracefile from test coverage data.
%
%   Depends on "test" which already ran the tests and wrote the HTML report
%   (and the underlying matlab.coverage.Result data).  This task converts
%   that data into Cobertura XML and lcov formats.

here   = fileparts(mfilename('fullpath'));
buildDir = resolve_ws('BUILD_DIR', here);
covDir   = resolve_ws('BUILD_COVERAGE_DIR', fullfile(buildDir, 'coverage'));

covXmlFile  = fullfile(covDir, 'cobertura.xml');
covLcovFile = fullfile(covDir, 'coverage.lcov');

if ~isfolder(covDir)
    mkdir(covDir);
end

% ---- Cobertura XML -------------------------------------------------------
% Re-use TestTask's addCodeCoverage to write the XML.  We run the tests a
% second time if the .xml isn't there yet (idempotent because tests pass).
testDir = fullfile(here, '..', '..', '..', 'tests', 'matlab');

import matlab.buildtool.tasks.TestTask
import matlab.unittest.TestRunner
import matlab.unittest.plugins.CodeCoveragePlugin
import matlab.unittest.plugins.codecoverage.CoberturaFormat

suite  = testsuite(testDir);
runner = TestRunner.withNoPlugins;
runner.addPlugin(CodeCoveragePlugin.forFolder(buildDir, ...
    Producing=CoberturaFormat(covXmlFile)));
results = runner.run(suite);

if any([results.Failed])
    error('coverageTask:testsFailed', 'Tests failed during coverage run.');
end
fprintf('  Cobertura XML → %s\n', covXmlFile);

% ---- lcov tracefile ------------------------------------------------------
% matlab.coverage.Result (R2024b+) can export to lcov.
% For older releases we produce a minimal stub so the file always exists.
matFile = fullfile(covDir, 'coverage.mat');

try
    % R2024b+: run tests with coverage result capture
    import matlab.coverage.Result %#ok<SIMPT>
    runner2 = TestRunner.withNoPlugins;
    runner2.addPlugin(CodeCoveragePlugin.forFolder(buildDir, ...
        Producing=matlab.unittest.plugins.codecoverage.CoverageResult(matFile)));
    runner2.run(suite);

    covResult = load(matFile).result; %#ok<NASGU>
    % Export to lcov format (R2024b+)
    write(covResult, covLcovFile);
    delete(matFile);
    fprintf('  lcov tracefile  → %s\n', covLcovFile);
catch
    % Fallback: write a minimal valid lcov stub so downstream tools don't fail
    fid = fopen(covLcovFile, 'w');
    if fid ~= -1
        fprintf(fid, 'TN:\nend_of_record\n');
        fclose(fid);
    end
    fprintf('  lcov stub       → %s  (upgrade to R2024b for full lcov)\n', ...
            covLcovFile);
end

end % coverageTask


function cleanTask(~)
%CLEAN  Remove BUILD_DIR and BUILD_COVERAGE_DIR.

here     = fileparts(mfilename('fullpath'));
buildDir = resolve_ws('BUILD_DIR', here);
covDir   = resolve_ws('BUILD_COVERAGE_DIR', fullfile(buildDir, 'coverage'));

cleaned = false;

% Never delete the source directory itself
if ~strcmp(buildDir, here) && isfolder(buildDir)
    rmdir(buildDir, 's');
    fprintf('  Removed: %s\n', buildDir);
    cleaned = true;
end

if isfolder(covDir)
    rmdir(covDir, 's');
    fprintf('  Removed: %s\n', covDir);
    cleaned = true;
end

% If buildDir == here, only remove the MEX binary and coverage subfolder
if strcmp(buildDir, here)
    mexExt  = mexext;
    mexFile = fullfile(here, ['ctoon_mex.' mexExt]);
    if isfile(mexFile)
        delete(mexFile);
        fprintf('  Removed: %s\n', mexFile);
        cleaned = true;
    end
end

if ~cleaned
    fprintf('  Nothing to clean.\n');
end

end % cleanTask


% =========================================================================
% Helper: read a variable from base workspace, fall back to default
% =========================================================================
function val = resolve_ws(name, default)
    if evalin('base', ['exist(''' name ''', ''var'')'])
        val = strtrim(evalin('base', name));
        if isempty(val), val = default; end
    else
        val = default;
    end
end
