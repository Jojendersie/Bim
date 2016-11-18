#pragma once

#include <string>
#include <vector>
#include <memory>

#include "light.hpp"
#include "camera.hpp"

namespace bim {

	/// A scenario is a named collection of lights and an camera.
	class Scenario
	{
	public:
		Scenario(std::string _name) : m_name(move(_name)) {}

		const std::string& getName() const { return m_name; }

		std::shared_ptr<Light> getLight(uint _index) const { return m_lights[_index]; }
		uint getNumLights() const { return (uint)m_lights.size(); }
		bool hasLight(const std::shared_ptr<Light>& _light)
		{
			for(auto & l : m_lights)
				if(l == _light) return true;
			return false;
		}

		/// Add the reference to a light. The light must be referenced in the
		/// scene too.
		void addLight(std::shared_ptr<Light> _light) { m_lights.push_back(move(_light)); }

		void setCamera(std::shared_ptr<Camera> _camera) { m_camera = move(_camera); }
		std::shared_ptr<Camera> getCamera() { return m_camera; }
		std::shared_ptr<const Camera> getCamera() const { return m_camera; }
	private:
		std::string m_name;
		// std::shared_ptr<Camera>
		std::vector<std::shared_ptr<Light>> m_lights;
		std::shared_ptr<Camera> m_camera;
	};
}
