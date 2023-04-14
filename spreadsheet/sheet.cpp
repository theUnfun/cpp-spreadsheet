#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

inline std::ostream& operator<<(std::ostream& output, const CellInterface::Value& value) {
	std::visit(
		[&](const auto& x) {
			output << x;
		},
		value);
	return output;
}

void Sheet::SetCell(Position pos, const std::string text) {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Wrong cell coordinates");
	}

	ResizeTable(pos);

	auto& cell = cells_[pos.row][pos.col];
	if (!cell) {
		cell = CreateCell(*this);
	}

	cell->Set(text);
}

const CellInterface* Sheet::GetCell(Position pos) const {
	return const_cast<Sheet*>(this)->GetCell(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Wrong cell coordinates");
	}
	if (!IsCellExists(pos)) {
		return nullptr;
	}
	return cells_[pos.row][pos.col].get();
}

void Sheet::ClearCell(Position pos) {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Wrong cell coordinates");
	}
	if (!IsCellExists(pos)) {
		return;
	}
	cells_[pos.row][pos.col].release();
	size_t col_index = pos.col;
	while (cells_[pos.row].size() > 0 && !cells_[pos.row][col_index]) {
		cells_[pos.row].pop_back();
		--col_index;
	}
	size_t row_index = cells_.size() > 0 ? cells_.size() - 1 : 0;
	while (cells_.size() > 0 && cells_[row_index].size() == 0) {
		cells_.pop_back();
		--row_index;
	}
	size_.rows = cells_.size();
	size_t max_col_size = 0;
	for (size_t i = 0; i < cells_.size(); ++i) {
		if (cells_[i].size() > max_col_size) {
			max_col_size = cells_[i].size();
		}
	}
	size_.cols = max_col_size;
}

Size Sheet::GetPrintableSize() const {
	return size_;
}

void Sheet::PrintValues(std::ostream& output) const {
	for (const auto& r : cells_) {
		bool first = true;
		for (const auto& c : r) {
			if (!first) {
				output << '\t';
			}
			first = false;
			if (c) {
				output << c->GetValue();
			}
		}
		for (size_t i = 1; i < (size_.cols - r.size()); ++i) {
			output << '\t';
		}
		output << '\n';
	}
}

void Sheet::PrintTexts(std::ostream& output) const {
	for (const auto& r : cells_) {
		bool first = true;
		for (const auto& c : r) {
			if (!first) {
				output << '\t';
			}
			first = false;
			if (c) {
				output << c->GetText();
			}
		}
		for (size_t i = 1; i < (size_.cols - r.size()); ++i) {
			output << '\t';
		}
		output << '\n';
	}
}

std::unique_ptr<SheetInterface> CreateSheet() {
	return std::make_unique<Sheet>();
}

void Sheet::ResizeTable(Position pos) {
	const int row = pos.row;
	const int col = pos.col;
	if (row >= static_cast<int>(cells_.size())) {
		cells_.resize(row + 1);
	}
	if (col >= static_cast<int>(cells_[row].size())) {
		cells_[row].resize(col + 1);
	}

	size_.rows = std::max(size_.rows, row + 1);
	size_.cols = std::max(size_.cols, col + 1);
}

bool Sheet::IsCellExists(Position pos) const {
	return static_cast<size_t>(pos.row) < cells_.size() && static_cast<size_t>(pos.col) < cells_[pos.row].size();
}