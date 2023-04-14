#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>

class Sheet;

std::ostream& operator<<(std::ostream& output, const CellInterface::Value& value);

class Cell : public CellInterface {
public:
	Cell(SheetInterface& sheet);
	~Cell();

	void Set(const std::string& text);

	void Clear();

	Value GetValue() const override;

	std::string GetText() const override;

	std::vector<Position> GetReferencedCells() const override;

	bool IsReferenced() const;

private:
	class Impl;
	class EmptyImpl;
	class TextImpl;
	class FormulaImpl;

	bool VisitCell(const CellInterface* current, std::unordered_set<const CellInterface*>& visited_cells, const std::unordered_set<const CellInterface*>& referenced_cells) const;

	bool IsCyclic(const Impl& impl) const;

	void InvalidateCache();

	void UpdateDependencies();

	bool IsFormula(const std::string& text) const;

	std::unique_ptr<FormulaImpl> CreateFormulaImpl(const std::string& formula) const;

private:
	std::unique_ptr<Impl> impl_;

	std::unordered_set<Cell*> childs_;

	std::unordered_set<Cell*> parents_;

	SheetInterface& sheet_;

};

std::unique_ptr<Cell> CreateCell(SheetInterface& sheet);