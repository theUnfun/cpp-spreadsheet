#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <stack>

using namespace std::literals;

class Cell::Impl {
public:
	Impl() = default;

	virtual ~Impl() = default;

	virtual Value GetValue() const = 0;

	virtual std::string GetText() const = 0;

	virtual std::vector<Position> GetReferencedCells() const {
		return {};
	}

	virtual void InvalidateCache() {};
};

class Cell::EmptyImpl : public Impl {
public:
	Value GetValue() const override {
		return ""s;
	}

	std::string GetText() const override {
		return ""s;
	}
};

class Cell::TextImpl : public Impl {
public:
	explicit TextImpl(std::string str) : text_(std::move(str)) {}

	Value GetValue() const override {
		if (text_[0] == ESCAPE_SIGN) {
			return text_.substr(1);
		}
		return text_;
	}

	std::string GetText() const override {
		return text_;
	}

private:
	std::string text_;
};

class Cell::FormulaImpl : public Impl {
public:
	explicit FormulaImpl(std::string str, const SheetInterface& sheet)
		: formula_(ParseFormula(std::move(str))), sheet_(sheet) {}

	Value GetValue() const override {
		if (!cache_.has_value()) {
			cache_ = formula_->Evaluate(sheet_);
		}
		return std::visit(
			[](const auto& formula_interface_value) { return Value(formula_interface_value); }, cache_.value());
	}

	std::string GetText() const override {
		return FORMULA_SIGN + formula_->GetExpression();
	}

	std::vector<Position> GetReferencedCells() const override {
		return formula_->GetReferencedCells();
	}

	void InvalidateCache() override {
		cache_.reset();
	}

private:
	std::unique_ptr<FormulaInterface> formula_;
	const SheetInterface& sheet_;
	mutable std::optional<FormulaInterface::Value> cache_;
};

Cell::Cell(SheetInterface& sheet) : impl_(std::make_unique<EmptyImpl>()), sheet_(sheet) {}

Cell::~Cell() {}

void Cell::Set(const std::string& text) {
	if (text.empty()) {
		impl_ = std::make_unique<EmptyImpl>();
		return;
	}
	if (IsFormula(text)) {
		impl_ = CreateFormulaImpl(text.substr(1));
		auto referenced_cells = impl_->GetReferencedCells();
		for (auto& cell : referenced_cells) {
			if (!sheet_.GetCell(cell)) {
				sheet_.SetCell(cell, {});
			}
		}
		UpdateDependencies();
		return;
	}
	impl_ = std::make_unique<TextImpl>(text);
}

void Cell::Clear() {
	Set("");
}

Cell::Value Cell::GetValue() const {
	return impl_->GetValue();
}

std::string Cell::GetText() const {
	return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
	return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
	return !parents_.empty();
}

bool Cell::VisitCell(const CellInterface* current, std::unordered_set<const CellInterface*>& visited_cells, const std::unordered_set<const CellInterface*>& referenced_cells) const {
	visited_cells.insert(current);
	if (referenced_cells.count(current) > 0) {
		return true;
	}
	auto& incoming_cells = static_cast<const Cell*>(current)->parents_;
	for (auto& incoming : incoming_cells) {
		if (visited_cells.find(incoming) == visited_cells.end()) {
			if (VisitCell(incoming, visited_cells, referenced_cells)) {
				return true;
			}
		}
	}
	return false;
}

bool Cell::IsCyclic(const Cell::Impl& impl) const {
	auto referenced_positions = impl.GetReferencedCells();
	if (referenced_positions.empty()) {
		return false;
	}
	std::unordered_set<const CellInterface*> referenced_cells;
	for (const auto& pos : referenced_positions) {
		referenced_cells.insert(sheet_.GetCell(pos));
	}
	std::unordered_set<const CellInterface*> visited_cells;
	return VisitCell(this, visited_cells, referenced_cells);

}

void Cell::UpdateDependencies() {
	std::for_each(childs_.begin(), childs_.end(), [this](Cell* outgoing) {
		outgoing->parents_.erase(this);
		});
	childs_.clear();
	for (const auto& pos : impl_->GetReferencedCells()) {
		auto outgoing = dynamic_cast<Cell*>(sheet_.GetCell(pos));
		childs_.insert(outgoing);
		outgoing->parents_.insert(this);
	}
	InvalidateCache();
}

void Cell::InvalidateCache() {
	impl_->InvalidateCache();
	for (auto in_ref : parents_) {
		in_ref->InvalidateCache();
	}
}

std::unique_ptr<Cell> CreateCell(SheetInterface& sheet) {
	return std::make_unique<Cell>(sheet);
}


bool Cell::IsFormula(const std::string& text) const {
	return text.size() > 1 && text[0] == FORMULA_SIGN;
}

std::unique_ptr<Cell::FormulaImpl> Cell::CreateFormulaImpl(const std::string& formula) const {
	auto impl = std::make_unique<FormulaImpl>(formula, sheet_);
	if (IsCyclic(*impl)) {
		throw CircularDependencyException("Circular dependency");
	}
	return impl;
}