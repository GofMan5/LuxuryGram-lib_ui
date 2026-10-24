// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/abstract_button.h"

#include "ui/integration.h"
#include "ui/round_rect.h"
#include "ui/ui_utility.h"

#include "styles/palette.h"
#include "styles/style_basic.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QtEvents>
#include <rpl/filter.h>
#include <rpl/mappers.h>

#include <algorithm>

namespace Ui {

AbstractButton::AbstractButton(QWidget *parent) : RpWidget(parent) {
	setMouseTracking(true);

	using namespace rpl::mappers;
	shownValue()
		| rpl::filter(_1 == false)
		| rpl::on_next([this] { clearState(); }, lifetime());
}

void AbstractButton::leaveEventHook(QEvent *e) {
	if (_state & StateFlag::Down) {
		return;
	}

	setOver(false, StateChangeSource::ByHover);
	return RpWidget::leaveEventHook(e);
}

void AbstractButton::enterEventHook(QEnterEvent *e) {
	checkIfOver(mapFromGlobal(QCursor::pos()));
	return RpWidget::enterEventHook(e);
}

void AbstractButton::setAcceptBoth(bool acceptBoth, bool triggerOnPress) {
	_acceptBoth = acceptBoth;
	_triggerOnPress = triggerOnPress;
}

void AbstractButton::checkIfOver(QPoint localPos) {
	auto over = rect().marginsRemoved(getMargins()).contains(localPos);
	setOver(over, StateChangeSource::ByHover);
}

void AbstractButton::mousePressEvent(QMouseEvent *e) {
	checkIfOver(e->pos());
	if (_state & StateFlag::Over) {
		const auto set = setDown(
			true,
			StateChangeSource::ByPress,
			e->modifiers(),
			e->button());
		if (set) {
			e->accept();
		}
	}
}

void AbstractButton::mouseMoveEvent(QMouseEvent *e) {
	if (rect().marginsRemoved(getMargins()).contains(e->pos())) {
		setOver(true, StateChangeSource::ByHover);
	} else {
		setOver(false, StateChangeSource::ByHover);
	}
}

void AbstractButton::mouseReleaseEvent(QMouseEvent *e) {
	const auto set = setDown(
		false,
		StateChangeSource::ByPress,
		e->modifiers(),
		e->button());
	if (set) {
		e->accept();
	}
}

bool AbstractButton::isSubmitEvent(not_null<QKeyEvent*> e) const {
	return !e->isAutoRepeat()
		&& (e->key() == Qt::Key_Space
			|| e->key() == Qt::Key_Return
			|| e->key() == Qt::Key_Enter);
}

void AbstractButton::keyPressEvent(QKeyEvent *e) {
	if (isSubmitEvent(e)) {
		setDown(
			true,
			StateChangeSource::ByPress,
			e->modifiers(),
			Qt::LeftButton);
		e->accept();
	} else {
		RpWidget::keyPressEvent(e);
	}
}

void AbstractButton::keyReleaseEvent(QKeyEvent *e) {
	if (isSubmitEvent(e)) {
		e->accept();
		if (isDown()) {
			const auto over = isOver();
			const auto weak = base::make_weak(this);
			setDown(
				false,
				StateChangeSource::ByPress,
				e->modifiers(),
				Qt::LeftButton);

			// setDown() already activates a hovered button on release.
			if (weak && !over && !isDisabled()) {
				clicked(e->modifiers(), Qt::LeftButton);
			}
		}
	} else {
		RpWidget::keyReleaseEvent(e);
	}
}

void AbstractButton::focusInEvent(QFocusEvent *e) {
	_keyboardFocus = (e->reason() != Qt::MouseFocusReason);
	RpWidget::focusInEvent(e);
	update();
}

void AbstractButton::focusOutEvent(QFocusEvent *e) {
	_keyboardFocus = false;
	const auto weak = base::make_weak(this);
	if (isDown()) {
		clearState();
	}
	if (weak) {
		RpWidget::focusOutEvent(e);
		update();
	}
}

void AbstractButton::paintKeyboardFocus(
		QPainter &p,
		QRect area,
		int radius,
		QColor color,
		std::optional<std::array<int, 4>> corners) const {
	if (!hasFocus() || !_keyboardFocus || isDisabled()) {
		return;
	}
	p.save();
	p.setOpacity(1.);
	p.setPen(QPen(color.isValid() ? color : st::windowFg->c, st::lineWidth));
	p.setBrush(Qt::NoBrush);
	p.setRenderHint(QPainter::Antialiasing, radius > 0 || corners.has_value());
	const auto outline = (area.isEmpty() ? rectNoMargins() : area)
		.marginsRemoved(QMargins(
			st::buttonFocusInset,
			st::buttonFocusInset,
			st::buttonFocusInset,
			st::buttonFocusInset));
	if (corners) {
		for (auto &corner : *corners) {
			corner = std::max(0, corner - st::buttonFocusInset);
		}
		p.drawPath(ComplexRoundedRectPath(
			outline,
			(*corners)[0],
			(*corners)[1],
			(*corners)[2],
			(*corners)[3]));
	} else {
		const auto innerRadius = std::max(0, radius - st::buttonFocusInset);
		p.drawRoundedRect(outline, innerRadius, innerRadius);
	}
	p.restore();
}

void AbstractButton::clicked(
		Qt::KeyboardModifiers modifiers,
		Qt::MouseButton button) {
	if (isDisabled()) {
		return;
	}
	_modifiers = modifiers;
	const auto weak = base::make_weak(this);
	if (button == Qt::LeftButton) {
		if (const auto callback = _clickedCallback) {
			callback();
		}
	}
	if (weak) {
		_clicks.fire_copy(button);
	}
}

void AbstractButton::setPointerCursor(bool enablePointerCursor) {
	if (_enablePointerCursor != enablePointerCursor) {
		_enablePointerCursor = enablePointerCursor;
		updateCursor();
	}
}

void AbstractButton::setOver(bool over, StateChangeSource source) {
	if (over == isOver()) {
		return;
	}
	const auto was = _state;
	if (over) {
		_state |= StateFlag::Over;
		Integration::Instance().registerLeaveSubscription(this);
	} else {
		_state &= ~State(StateFlag::Over);
		Integration::Instance().unregisterLeaveSubscription(this);
	}
	const auto weak = base::make_weak(this);
	onStateChanged(was, source);
	if (weak) {
		updateCursor();
		update();
	}
}

bool AbstractButton::setDown(
		bool down,
		StateChangeSource source,
		Qt::KeyboardModifiers modifiers,
		Qt::MouseButton button) {
	if (down
		&& !isDisabled()
		&& !(_state & StateFlag::Down)
		&& (_acceptBoth || button == Qt::LeftButton)) {
		auto was = _state;
		_state |= StateFlag::Down;

		const auto weak = base::make_weak(this);
		onStateChanged(was, source);
		if (weak) {
			accessibilityStateChanged({ .pressed = true });
		}
		if (weak && _triggerOnPress && button != Qt::LeftButton) {
			_state &= ~State(StateFlag::Down);
			clicked(modifiers, button);
		}
		return true;
	} else if (!down && (_state & StateFlag::Down)) {
		const auto was = _state;
		_state &= ~State(StateFlag::Down);

		const auto weak = base::make_weak(this);
		onStateChanged(was, source);
		if (weak) {
			accessibilityStateChanged({ .pressed = true });
			if (!weak) {
				return true;
			} else if (was & StateFlag::Over) {
				clicked(modifiers, button);
			} else {
				setOver(false, source);
			}
		}
		return true;
	}
	return false;
}

void AbstractButton::updateCursor() {
	const auto pointerCursor = _enablePointerCursor && isOver() && !isDisabled();
	if (_pointerCursor != pointerCursor) {
		_pointerCursor = pointerCursor;
		setCursor(_pointerCursor ? style::cur_pointer : style::cur_default);
	}
}

void AbstractButton::setDisabled(bool disabled) {
	if (disabled == isDisabled()) {
		return;
	}
	const auto was = _state;
	if (disabled) {
		_state |= StateFlag::Disabled;
		_state &= ~State(StateFlag::Down);
	} else {
		_state &= ~State(StateFlag::Disabled);
	}
	const auto weak = base::make_weak(this);
	onStateChanged(was, StateChangeSource::ByUser);
	if (weak) {
		auto changes = QAccessible::State();
		changes.disabled = true;
		changes.pressed = !!(was & StateFlag::Down);
		QAccessibleStateChangeEvent event(this, changes);
		QAccessible::updateAccessibility(&event);
		if (weak) {
			updateCursor();
			update();
		}
	}
}

void AbstractButton::clearState() {
	const auto was = _state;
	_state &= StateFlag::Disabled;
	if (was & StateFlag::Over) {
		Integration::Instance().unregisterLeaveSubscription(this);
	}
	const auto weak = base::make_weak(this);
	onStateChanged(was, StateChangeSource::ByUser);
	if (weak && (was & StateFlag::Down)) {
		accessibilityStateChanged({ .pressed = true });
	}
}

AccessibilityState AbstractButton::accessibilityState() const {
	return { .pressed = isDown() };
}

void AbstractButton::accessibilityDoAction(const QString &name) {
	if (name == QAccessibleActionInterface::pressAction()) {
		if (!isDisabled()) {
			clicked(Qt::NoModifier, Qt::LeftButton);
		}
	}
}

} // namespace Ui
