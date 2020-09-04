#include "fmain.h"
#include "ui_fmain.h"

fmain::fmain(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::fmain)
{
    ui->setupUi(this);

    // Set up node handle.
    fmain::m_node = new ros::NodeHandle("~");

    // Read parameters.
    fmain::p_max_data_rate = fmain::m_node->param<double>("max_data_rate", 10.0);

    // Set up optimizer.
    fmain::m_optimizer_fit = new qn_optimizer(9, std::bind(&fmain::objective_fit, this, std::placeholders::_1));

    // Initialize state.
    fmain::m_state = fmain::state_t::IDLE;

    // Initialize charts.
    fmain::initialize_charts();

    // Set up magnetometer plot combobox.
    fmain::ui->combobox_charts->addItems({"XY", "XZ", "YZ"});
    fmain::ui->combobox_charts->setCurrentIndex(0);

    // Start ros spinner.
    connect(&(fmain::m_ros_spinner), &QTimer::timeout, this, &fmain::ros_spin);
    fmain::m_ros_spinner.start(10);
}

fmain::~fmain()
{
    // Cancel any running collection.
    fmain::stop_collection();

    // Clear any collection.
    fmain::clear_collection();

    // Clear charts.
    fmain::clear_charts();

    // Clean up node.
    delete fmain::m_node;

    // Clean up UI.
    delete ui;
}

// ROS
void fmain::ros_spin()
{
    // Handle callbacks.
    ros::spinOnce();

    // Quit if ROS shutting down.
    if(!ros::ok())
    {
        QApplication::quit();
    }
}


void fmain::on_button_start_collection_clicked()
{
    fmain::start_collection();
}

void fmain::on_button_stop_collection_clicked()
{
    fmain::stop_collection();
}

void fmain::on_combobox_charts_currentIndexChanged(int index)
{
    fmain::chart_t chart_type = static_cast<fmain::chart_t>(index);
    if(fmain::m_charts.count(chart_type))
    {
        fmain::ui->chartview->setChart(fmain::m_charts[chart_type]);
    }
}

void fmain::on_button_clear_collection_clicked()
{
    fmain::clear_collection();
}


void fmain::start_collection()
{
    if(fmain::m_state == fmain::state_t::IDLE)
    {
        // Enable point timer.
        fmain::m_point_timer.start();

        // Subscribe to magnetometer data stream.
        ros::NodeHandle public_handle;
        fmain::m_subscriber = public_handle.subscribe("/imu/magnetometer", 100, &fmain::subscriber, this);

        // Update state.
        fmain::m_state = fmain::state_t::COLLECTION;
    }
}
void fmain::stop_collection()
{
    if(fmain::m_state == fmain::state_t::COLLECTION)
    {
        // Close down subscriber.
        fmain::m_subscriber.shutdown();

        // Stop point timer.
        fmain::m_point_timer.invalidate();

        // Update state.
        fmain::m_state = fmain::state_t::IDLE;
    }
}
void fmain::clear_collection()
{
    if(fmain::m_state == fmain::state_t::IDLE || fmain::m_state == fmain::state_t::COLLECTION)
    {
        // Clear deque.
        fmain::m_points.clear();

        // Update charts.
        fmain::update_charts();

        // Clear form's point counter.
        fmain::ui->lineedit_n_collection_points->setText(0);
    }
    // Clean up points.
    for(auto point = fmain::m_points.cbegin(); point != fmain::m_points.cend(); ++point)
    {
        delete (*point);
    }
}

void fmain::start_fit()
{
    if(fmain::m_state == fmain::state_t::IDLE)
    {
        // Set up variable vector with initial guess.
        // Use c = 0,0,0 and A = identity (ortho axes)
        Eigen::VectorXd fit;
        fit.setZero(9);
        fit(3) = 1.0;
        fit(6) = 1.0;
        fit(8) = 1.0;
//        // Use point mid as initial guess for xc, yc, zc.
//        // Use point range as initial guess for a,b,c.
//        double x_min = std::numeric_limits<double>::max();
//        double x_max = -std::numeric_limits<double>::max();
//        double y_min = std::numeric_limits<double>::max();
//        double y_max = -std::numeric_limits<double>::max();
//        double z_min = std::numeric_limits<double>::max();
//        double z_max = -std::numeric_limits<double>::max();
//        for(auto point_entry = fmain::m_points.cbegin(); point_entry != fmain::m_points.cend(); ++point_entry)
//        {
//            fmain::point_t* point = *point_entry;
//            if(point->x < x_min)
//            {
//                x_min = point->x;
//            }
//            if(point->x > x_max)
//            {
//                x_max = point->x;
//            }
//            if(point->y < y_min)
//            {
//                y_min = point->y;
//            }
//            if(point->y > y_max)
//            {
//                y_max = point->y;
//            }
//            if(point->z < z_min)
//            {
//                z_min = point->z;
//            }
//            if(point->z > z_max)
//            {
//                z_max = point->z;
//            }
//        }
//        fit(0) = (x_min + x_max) / 2.0;
//        fit(1) = (y_min + y_max) / 2.0;
//        fit(2) = (z_min + z_max) / 2.0;
//        fit(3) = x_max - x_min;
//        fit(4) = y_max - y_min;
//        fit(5) = z_max - z_min;

        // Start the optimizer.
        fmain::m_optimizer_fit->p_max_iterations = 100;
        fmain::m_optimizer_fit->p_max_step_iterations = 10;
        fmain::m_optimizer_fit->p_initial_step_size = 0.001;
        fmain::m_optimizer_fit->optimize(fit);
        fmain::setWindowTitle(QString::number(fit(0)));

        // Update state.
        fmain::m_state = fmain::state_t::FIT;
    }
}
void fmain::stop_fit()
{
    if(fmain::m_state == fmain::state_t::FIT)
    {
        // Update state.
        fmain::m_state = fmain::state_t::IDLE;
    }
}

void fmain::initialize_charts()
{
    // Clear any existing charts.
    fmain::clear_charts();

    // Create a new chart for each of the 6 views.
    for(uint32_t i = 0; i < 3; ++i)
    {
        // Create chart.
        QtCharts::QChart* chart = new QtCharts::QChart();
        chart->legend()->hide();

        // Create collection series.
        QtCharts::QScatterSeries* series_collection = new QtCharts::QScatterSeries();
        series_collection->setColor(QColor(Qt::GlobalColor::blue));
        series_collection->setBorderColor(Qt::GlobalColor::transparent);
        series_collection->setMarkerSize(5);
        chart->addSeries(series_collection);
        // Create current position series.
        QtCharts::QScatterSeries* series_current_postion = new QtCharts::QScatterSeries();
        series_current_postion->setColor(QColor(Qt::GlobalColor::red));
        chart->addSeries(series_current_postion);

        // Set up axes.
        QtCharts::QValueAxis* axis_x = new QtCharts::QValueAxis();
        axis_x->setLabelFormat("%.2E");
        chart->addAxis(axis_x, Qt::AlignBottom);
        series_collection->attachAxis(axis_x);
        series_current_postion->attachAxis(axis_x);
        QtCharts::QValueAxis* axis_y = new QtCharts::QValueAxis();
        axis_y->setLabelFormat("%.2E");
        chart->addAxis(axis_y, Qt::AlignLeft);
        series_collection->attachAxis(axis_y);
        series_current_postion->attachAxis(axis_y);

        // Store chart and series.
        fmain::chart_t chart_type = static_cast<fmain::chart_t>(i);
        fmain::m_charts[chart_type] = chart;
        fmain::m_axes_x[chart_type] = axis_x;
        fmain::m_axes_y[chart_type] = axis_y;
        fmain::m_series_collections[chart_type] = series_collection;
        fmain::m_series_current_position[chart_type] = series_current_postion;
    }

    // Set plot specific settings.
    // XY
    fmain::m_axes_x[fmain::chart_t::XY]->setTitleText("x");
    fmain::m_axes_y[fmain::chart_t::XY]->setTitleText("y");
    // XZ
    fmain::m_axes_x[fmain::chart_t::XZ]->setTitleText("x");
    fmain::m_axes_y[fmain::chart_t::XZ]->setTitleText("z");
    // YZ
    fmain::m_axes_x[fmain::chart_t::YZ]->setTitleText("y");
    fmain::m_axes_y[fmain::chart_t::YZ]->setTitleText("z");

}
void fmain::clear_charts()
{
    // Clean up charts. NOTE: The charts have ownership of all included pointers.
    for(uint32_t i = 0; i < 3; ++i)
    {
        fmain::chart_t chart_type = static_cast<fmain::chart_t>(i);
        if(fmain::m_charts.count(chart_type))
        {
            delete fmain::m_charts[chart_type];
        }
    }

    // Clear out chart vectors.
    fmain::m_charts.clear();
    fmain::m_axes_x.clear();
    fmain::m_axes_y.clear();
    fmain::m_series_collections.clear();
    fmain::m_series_current_position.clear();
}
void fmain::update_charts()
{
    // Iterate through all points to set up xy, xz, and yz vectors.
    QVector<QPointF> points_xy;
    QVector<QPointF> points_xz;
    QVector<QPointF> points_yz;
    double x_min = std::numeric_limits<double>::max();
    double x_max = -std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::max();
    double y_max = -std::numeric_limits<double>::max();
    double z_min = std::numeric_limits<double>::max();
    double z_max = -std::numeric_limits<double>::max();
    for(auto point_entry = fmain::m_points.cbegin(); point_entry != fmain::m_points.cend(); ++point_entry)
    {
        fmain::point_t* point = *point_entry;

        // Add points to vector.
        points_xy.append(QPointF(point->x, point->y));
        points_xz.append(QPointF(point->x, point->z));
        points_yz.append(QPointF(point->y, point->z));

        // Calculate data ranges.
        // X
        if(point->x < x_min)
        {
            x_min = point->x;
        }
        if(point->x > x_max)
        {
            x_max = point->x;
        }
        // Y
        if(point->y < y_min)
        {
            y_min = point->y;
        }
        if(point->y > y_max)
        {
            y_max = point->y;
        }
        // Z
        if(point->z < z_min)
        {
            z_min = point->z;
        }
        if(point->z > z_max)
        {
            z_max = point->z;
        }
    }

    // Update collection series.
    fmain::m_series_collections[fmain::chart_t::XY]->replace(points_xy);
    fmain::m_series_collections[fmain::chart_t::XZ]->replace(points_xz);
    fmain::m_series_collections[fmain::chart_t::YZ]->replace(points_yz);

    // Update current point series.
    if(!fmain::m_points.empty())
    {
        // Create vectors.
        QVector<QPointF> points_xy_current;
        QVector<QPointF> points_xz_current;
        QVector<QPointF> points_yz_current;
        // Populate vectors.
        auto current_point = fmain::m_points.back();
        points_xy_current.append(QPointF(current_point->x, current_point->y));
        points_xz_current.append(QPointF(current_point->x, current_point->z));
        points_yz_current.append(QPointF(current_point->y, current_point->z));
        // Add vectors to series.
        fmain::m_series_current_position[fmain::chart_t::XY]->replace(points_xy_current);
        fmain::m_series_current_position[fmain::chart_t::XZ]->replace(points_xz_current);
        fmain::m_series_current_position[fmain::chart_t::YZ]->replace(points_yz_current);
    }
    else
    {
        fmain::m_series_current_position[fmain::chart_t::XY]->clear();
        fmain::m_series_current_position[fmain::chart_t::XZ]->clear();
        fmain::m_series_current_position[fmain::chart_t::YZ]->clear();
    }

    // Update scale.
    if(!fmain::m_points.empty())
    {
        // Determine the center of the plot ranges.
        double x_avg = (x_min + x_max) / 2.0;
        double y_avg = (y_min + y_max) / 2.0;
        double z_avg = (z_min + z_max) / 2.0;
        // Iterate over charts.
        for(uint32_t i = 0; i < 3; ++i)
        {
            fmain::chart_t chart_type = static_cast<fmain::chart_t>(i);

            // Get the chart's current plot area size to figure out aspect ratio.
            QRectF plot_area = fmain::m_charts[chart_type]->plotArea();
            double aspect_ratio = plot_area.width() / plot_area.height();
            // Calculate ranges based on aspect ratio.
            double xh_min, xh_max, xh_avg, xh_range;
            double yh_min, yh_max, yh_avg, yh_range;
            switch(chart_type)
            {
                case fmain::chart_t::XY:
                {
                    xh_min = x_min;
                    xh_max = x_max;
                    yh_min = y_min;
                    yh_max = y_max;
                    xh_avg = x_avg;
                    yh_avg = y_avg;
                    break;
                }
                case fmain::chart_t::XZ:
                {
                    xh_min = x_min;
                    xh_max = x_max;
                    yh_min = z_min;
                    yh_max = z_max;
                    xh_avg = x_avg;
                    yh_avg = z_avg;
                    break;
                }
                case fmain::chart_t::YZ:
                {
                    xh_min = y_min;
                    xh_max = y_max;
                    yh_min = z_min;
                    yh_max = z_max;
                    xh_avg = y_avg;
                    yh_avg = z_avg;
                    break;
                }
            }

            if(aspect_ratio > 1.0)
            {
                yh_range = (yh_max - yh_min) * 1.2;
                xh_range = yh_range * aspect_ratio;
            }
            else
            {
                xh_range = (xh_max - xh_min) * 1.2;
                yh_range = xh_range / aspect_ratio;
            }
            // Set axis ranges.
            fmain::m_axes_x[chart_type]->setRange(xh_avg - xh_range/2.0, xh_avg + xh_range/2.0);
            fmain::m_axes_y[chart_type]->setRange(yh_avg - yh_range/2.0, yh_avg + yh_range/2.0);
        }
    }
}

// SUBSCRIBERS
void fmain::subscriber(const sensor_msgs_ext::magnetometerConstPtr &message)
{
    // Enforce max data rate.
    if(fmain::m_point_timer.elapsed() >= 1000.0/fmain::p_max_data_rate)
    {
        // Reset point timer.
        fmain::m_point_timer.restart();

        // Convert message into new point instance.
        point_t* point = new point_t();
        point->x = message->x;
        point->y = message->y;
        point->z = message->z;

        // Add point to calibration.
        fmain::m_points.push_back(point);

        // Update charts.
        fmain::update_charts();

        // Update form's point counter.
        fmain::ui->lineedit_n_collection_points->setText(QString::number(fmain::m_points.size()));
    }
}

// OPTIMIZATION FUNCTIONS
double fmain::objective_fit(const Eigen::VectorXd& variables)
{
    // Calculate mean squared error of generalized ellipsoid function:
    // (x-c)' * A * (x-c) = 1
    // d = (x-c)

    // Create c vector.
    fmain::m_c(0) = variables(0);
    fmain::m_c(1) = variables(1);
    fmain::m_c(2) = variables(2);

    // Create A Matrix.
    fmain::m_a(0,0) = variables(3);
    fmain::m_a(0,1) = variables(4);
    fmain::m_a(1,0) = variables(4);
    fmain::m_a(0,2) = variables(5);
    fmain::m_a(2,0) = variables(5);
    fmain::m_a(1,1) = variables(6);
    fmain::m_a(1,2) = variables(7);
    fmain::m_a(2,1) = variables(7);
    fmain::m_a(2,2) = variables(8);

    // Set up MSE to sum over all points.
    double mse = 0.0;

    // Iterate over each point in the collection.
    for(auto point = fmain::m_points.cbegin(); point != fmain::m_points.cend(); ++point)
    {
        // Calculate difference vector and its transpose.
        fmain::m_d(0) = (*point)->x - fmain::m_c(0);
        fmain::m_d(1) = (*point)->y - fmain::m_c(1);
        fmain::m_d(2) = (*point)->z - fmain::m_c(2);
        fmain::m_dt.noalias() = fmain::m_d.transpose();

        // Calculate actual value.
        fmain::m_t1.noalias() = fmain::m_dt * fmain::m_a;
        fmain::m_t2.noalias() = fmain::m_t1 * fmain::m_d;

        // Calculate MSE and add to sum.
        mse += std::pow(fmain::m_t2(0) - 1.0, 2.0);
    }

    // Return MSE.
    return mse;
}

void fmain::on_button_start_fit_clicked()
{
    fmain::start_fit();
}