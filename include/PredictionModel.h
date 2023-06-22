#ifndef PREDICTIONMODEL_H
#define PREDICTIONMODEL_H
#include <AbstractTask.h>
#include <ResearchScope.h>
#include <TimeSeriesExtraction.h>
#include <Eigen/Dense>
#include <TFModel.h>

class PredictionModel: public AbstractTask
{
    public:
        PredictionModel(const std::string path, const std::string kws, const std::string modelFileName, TimeSeriesExtraction *tse);
        virtual ~PredictionModel();
        virtual bool finished();
        virtual const char *name();
        virtual int numSteps();
        virtual void doStep(int stepId);
        bool load(int y, std::map<uint64_t, std::pair<Eigen::MatrixXd,Eigen::MatrixXd>> *prediction);

    protected:
        bool save(int y, std::map<uint64_t, std::pair<Eigen::MatrixXd,Eigen::MatrixXd>> &prediction);
        bool process();

    private:
        int _y0;
        int _y1;
        int _y2;
        ResearchScope _scope;
        TimeSeriesExtraction *_tse;
        TFModel _model;
};

#endif // PREDICTIONMODEL_H
