#pragma once

#include <string>
#include <vector>
#include <memory>

#include "light.hpp"

namespace bim {

	/// A scenario is a named collection of lights and an camera.
	class Scenario
	{
	public:
		Scenario(std::string _name) : m_name(move(_name)) {}

		const std::string& getName() const { return m_name; }

		std::shared_ptr<Light> getLight(uint _index) const { return m_lights[_index]; }
		uint getNumLights() const { return (uint)m_lights.size(); }

		/// Add the reference to a light. The light must be referenced in the
		/// scene too.
		void addLight(std::shared_ptr<Light> _light) { m_lights.push_back(move(_light)); }
	private:
		std::string m_name;
		// std::shared_ptr<Camera>
		std::vector<std::shared_ptr<Light>> m_lights;
	};
}
