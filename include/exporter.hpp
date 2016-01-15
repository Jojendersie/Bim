#pragma once

namespace bim {

	// Exports all chunks into a Wavefront .obj file
	void exportToObj(BinaryModel& _model, const char* _fileName);

} // namespace bim