#include "AbstractTask.h"

std::atomic<bool> AbstractTask::_cancelled(false);
std::thread *AbstractTask::_taskThread(NULL);
ProgressReporter *AbstractTask::_progressReporter(NULL);

AbstractTask::AbstractTask()
{
    //ctor
    _prev = NULL;
    _next = NULL;
}

AbstractTask::~AbstractTask()
{
    //dtor
    finalize();
}

void AbstractTask::finalize()
{
    if (_taskThread != NULL)
    {
        _taskThread->join();
        delete _taskThread;
        _taskThread = NULL;
    }
    _cancelled.store(false);
}

void AbstractTask::runAll()
{
    finalize();
    if (numSteps() > 0 && !finished())
    {
        _taskThread = new std::thread([this]()
            {
                AbstractTask *task = this;
                while (task != NULL)
                {
                    if (_cancelled.load() == true)
                    {
                        if (_progressReporter != NULL)
                        {
                            _progressReporter->report("Cancelled", getTaskId(), getNumTasks(), 0);
                        }
                        _cancelled.store(false);
                        return;
                    }
                    if (_progressReporter != NULL)
                    {
                        _progressReporter->report(name(), getTaskId(), getNumTasks(), 0);
                    }
                    int n = numSteps();
                    for (int stepId = 0; stepId < n; stepId++)
                    {
                        doStep(stepId);
                        int taskProgress = 100 * (stepId + 1) / n;

                        if (_progressReporter != NULL)
                        {
                            if (_cancelled.load() == true)
                                _progressReporter->report("Cancelled", getTaskId(), getNumTasks(), taskProgress);
                            else
                                _progressReporter->report(name(), getTaskId(), getNumTasks(), taskProgress);
                        }
                        if (_cancelled.load() == true)
                        {
                            _cancelled.store(false);
                            return;
                        }
                    }
                    task = task->_next;
                }
                if (_progressReporter != NULL)
                    _progressReporter->report("Done", getNumTasks(), getNumTasks(), 0);
            });
    }
    else if (_next != NULL)
    {
        _next->runAll();
    }
    else if (_progressReporter != NULL)
    {
        _progressReporter->report("Done", getNumTasks(), getNumTasks(), 100);
    }
}

void AbstractTask::cancel()
{
    if (_taskThread != NULL)
    {
        _cancelled.store(true);
    }
}

void AbstractTask::setProgressReporter(ProgressReporter *value)
{
    _progressReporter = value;
}
