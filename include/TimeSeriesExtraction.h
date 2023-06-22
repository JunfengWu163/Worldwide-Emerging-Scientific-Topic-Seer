#ifndef TimeSeriesEXTRACTION_H
#define TimeSeriesEXTRACTION_H
#include <AbstractTask.h>
#include <ResearchScope.h>
#include <BitermWeight.h>
#include <CandidateIdentification.h>
#include <TopicIndentification.h>
#include <Eigen/Dense>
#include <map>

typedef std::pair<
    std::pair<Eigen::MatrixXd,Eigen::MatrixXd>,
    std::pair<Eigen::MatrixXd,Eigen::MatrixXd>
    > TimeSeriesMatrices;

class TimeSeriesExtraction: public AbstractTask
{
    public:
        TimeSeriesExtraction(const std::string path, const std::string kws,
                               BitermWeight *bw, CandidateIdentification *ci, TopicIndentification *ti);
        virtual ~TimeSeriesExtraction();
        virtual bool finished();
        virtual const char *name();
        virtual int numSteps();
        virtual void doStep(int stepId);
        bool load(int y, std::map<uint64_t, TimeSeriesMatrices> *timeSeries);

    protected:
        bool save(int y, const std::map<uint64_t, TimeSeriesMatrices> &timeSeries);
        bool process(int y);

    private:
        int _y0;
        int _y1;
        int _y2;
        ResearchScope _scope;
        BitermWeight *_bw;
        CandidateIdentification *_ci;
        TopicIndentification *_ti;
};

#endif // TimeSeriesEXTRACTION_H
