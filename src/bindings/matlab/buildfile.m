function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon MATLAB MEX binding.
%
%   Run from the project root or any subfolder:
%
%       buildtool              % default task: test (mex -> test)
%       buildtool mex          % compile ctoon_mex only
%       buildtool test         % compile + run tests + HTML coverage report
%       buildtool coverage     % compile + run tests + Cobertura XML report
%       buildtool -tasks       % list available tasks
%
%   Requirements:
%       MATLAB R2022b+  (Build Tool)
%       MATLAB R2024a+  (addCodeCoverage)
%       A C compiler configured for MEX  (run `mex -setup C` once)
%
%   See also: buildtool, matlab.buildtool.tasks.TestTask

import matlab.buildtool.tasks.TestTask

% -------------------------------------------------------------------------
% Paths (relative to this buildfile, which lives in src/bindings/matlab)
% -------------------------------------------------------------------------
here    = fileparts(mfilename('fullpath'));   % …/src/bindings/matlab
testDir = fullfile(here, '..', '..', '..', 'tests', 'matlab');

covHtmlDir = fullfile(here, 'coverage', 'html');
covXmlFile = fullfile(here, 'coverage', 'cobertura.xml');

% -------------------------------------------------------------------------
% Build plan
% -------------------------------------------------------------------------
plan = buildplan(localfunctions);

% "test" — run tests, collect coverage over the .m wrappers in this folder
plan("test") = TestTask(testDir, SourceFiles=here) ...
    .addCodeCoverage([covHtmlDir filesep 'index.html']);

% "coverage" — same as test but also writes Cobertura XML (for CI)
plan("coverage") = TestTask(testDir, SourceFiles=here) ...
    .addCodeCoverage(["code-coverage/report.html", covXmlFile]);

% "mex" must run before "test" and "coverage"
plan("test").Dependencies     = "mex";
plan("coverage").Dependencies = "mex";

% Default task
plan.DefaultTasks = "test";

end % buildfile


% =========================================================================
% Local task functions
% =========================================================================

function mexTask(~)
%MEX  Compile ctoon_mex.c + ctoon.c into a MEX binary.
%
%   JSON support is enabled by default (CTOON_ENABLE_JSON=1).
%   The resulting binary and all .m wrappers land in the same folder as
%   this buildfile so that a single addpath() is enough to use the binding.

here     = fileparts(mfilename('fullpath'));
repoRoot = fullfile(here, '..', '..', '..');
incDir   = fullfile(repoRoot, 'include');
mexSrc   = fullfile(here,   'ctoon_mex.c');
libSrc   = fullfile(repoRoot, 'src', 'ctoon.c');

assert(isfile(mexSrc), 'mexTask: ctoon_mex.c not found at %s', mexSrc);
assert(isfile(libSrc), 'mexTask: ctoon.c not found at %s',     libSrc);

if ispc()
    cflags = 'COMPFLAGS=$COMPFLAGS /DCTOON_ENABLE_JSON=1';
else
    cflags = 'CFLAGS=$CFLAGS -DCTOON_ENABLE_JSON=1';
end

fprintf('  Compiling ctoon_mex ...\n');
mex(mexSrc, libSrc, ['-I', incDir], '-outdir', here, cflags);
fprintf('  Done → %s\n', here);

end % mexTask
