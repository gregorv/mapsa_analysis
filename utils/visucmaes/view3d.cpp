#include "view3d.h"
#include "ui_view3d.h"
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/Qt3DWindow>

View3D::View3D(QWidget* parent) :
 QMainWindow(parent), ui(new Ui::View3D), _cache(nullptr)
{
	ui->setupUi(this);
	connect(ui->eventFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int idx) {
		ui->view->setEventFilter(static_cast<Viewport::event_filter_t>(idx));
	});
	connect(ui->phi, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->phi_2->setValue(val*180/3.141); });
	connect(ui->theta, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->theta_2->setValue(val*180/3.141); });
	connect(ui->omega, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->omega_2->setValue(val*180/3.141); });
	connect(ui->phi_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->phi->setValue(val*3.141/180); });
	connect(ui->theta_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->theta->setValue(val*3.141/180); });
	connect(ui->omega_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double val){ ui->omega->setValue(val*3.141/180); });
}

View3D::~View3D()
{
}

void View3D::hideEvent(QHideEvent* evt)
{
	QMainWindow::hideEvent(evt);
	emit visibleChanged(false);
	emit invisibleChanged(true);
}

void View3D::showEvent(QShowEvent* evt)
{
	QMainWindow::showEvent(evt);
	emit visibleChanged(true);
	emit invisibleChanged(false);
}

void View3D::setCache(const Database::run_cache_t* cache)
{
	_cache = cache;
	ui->view->setCache(cache);
	ui->run_ids->clear();
	for(const auto& key: cache->keys()) {
		ui->run_ids->addItem(QString::number(key));
	}
}

void View3D::selectRunIndex(QString index)
{
	if(!_cache) return;
	ui->view->setRunId(index.toInt());
	ui->event->setValue(1);
	ui->eventSlider->setValue(1);
}

void View3D::setNumEvents(int numEvents)
{
	ui->event->setMinimum(1);
	ui->eventSlider->setMinimum(1);
	ui->event->setMaximum(numEvents);
	ui->numEvents->setNum(static_cast<int>(numEvents));
	ui->eventSlider->setMaximum(numEvents);
}

