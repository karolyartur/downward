import logging
import os
import sys

from . import aliases
from . import arguments
from . import cleanup
from . import run_components
from . import __version__

from fast_downward_msgs.srv import CallFastDownward, CallFastDownwardResponse

def handle_planning_request(request):
    # logging.basicConfig(level=getattr(logging, request.log_level.upper()),
    #                     format="%(levelname)-8s %(message)s",
    #                     stream=sys.stdout)
    # logging.debug("processed args: %s" % request)

    if hasattr(request, "version"):
        if request.version:
            print(__version__)
            sys.exit()

    if hasattr(request, "show_aliases"):
        if request.show_aliases:
            aliases.show_aliases()
            sys.exit()

    if hasattr(request, "cleanup"):
        if request.cleanup:
            cleanup.cleanup_temporary_files(request)
            sys.exit()

    exitcode = None
    if request.run_all:
        request.components = ["translate", "search"]
        
    for component in request.components:
        if component == "translate":
            (exitcode, continue_execution) = run_components.run_translate(request)
        elif component == "search":
            if 'rospy' in sys.modules:
                    request.portfolio_bound = None
                    request.portfolio_single_plan = False
                    request.portfolio = None
            (exitcode, continue_execution) = run_components.run_search(request)
            if hasattr(request, "keep_sa_file"):
                if not request.keep_sas_file:
                    print("Remove intermediate file {}".format(request.sas_file))
                    os.remove(request.sas_file)
            else:
                print("Remove intermediate file {}".format(request.sas_file))
                os.remove(request.sas_file)
        elif component == "validate":
            (exitcode, continue_execution) = run_components.run_validate(request)
        else:
            assert False, "Error: unhandled component: {}".format(component)
        print("{component} exit code: {exitcode}".format(**locals()))
        print()
        if not continue_execution:
            print("Driver aborting after {}".format(component))
            break
    return CallFastDownwardResponse(exitcode)


def fast_downward_server():

    import rospy

    rospy.init_node('fast_downward_server')
    service = rospy.Service('fast_downward', CallFastDownward, handle_planning_request)
    rospy.loginfo("Fast-Downward server started")
    rospy.spin()


def main():
    args = arguments.parse_args()
    result = handle_planning_request(args)
    # Exit with the exit code of the last component that ran successfully.
    # This means for example that if no plan was found, validate is not run,
    # and therefore the return code is that of the search.
    sys.exit(result.exitcode)


if __name__ == "__main__":
    main()
