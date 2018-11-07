import pytest

def pytest_addoption(parser):
    parser.addoption("--output-dir",
                     required=True,
                     help="Test output folder")
    parser.addoption("--riscvemu",
                     required=True,
                     help="Path to riscvemu")

@pytest.fixture
def output_dir(request):
    return request.config.getoption("--output-dir")

@pytest.fixture
def riscvemu(request):
    return request.config.getoption("--riscvemu")
