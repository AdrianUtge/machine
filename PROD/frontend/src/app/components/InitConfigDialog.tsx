import React, { useState } from "react";

interface InitConfig {
  target_position_mm: number;
  descent_rate_mm_per_min: number;
  max_duration_s: number;
  auto_init_interval: number;
}

interface InitConfigDialogProps {
  config: InitConfig;
  onSave: (config: InitConfig) => Promise<void>;
  onCancel: () => void;
}

const InitConfigDialog: React.FC<InitConfigDialogProps> = ({
  config,
  onSave,
  onCancel,
}) => {
  const [formData, setFormData] = useState<InitConfig>(config);
  const [loading, setLoading] = useState(false);

  const handleChange = (
    e: React.ChangeEvent<HTMLInputElement | HTMLSelectElement>
  ) => {
    const { name, value } = e.target;
    const numValue = name === "auto_init_interval" ? parseInt(value) : parseFloat(value);
    setFormData((prev) => ({
      ...prev,
      [name]: numValue,
    }));
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);
    try {
      await onSave(formData);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <div className="bg-white rounded-lg shadow-lg p-6 max-w-md w-full">
        <h2 className="text-xl font-semibold mb-4">Init Configuration</h2>

        <form onSubmit={handleSubmit} className="space-y-4">
          {/* Target Position */}
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Target Position (mm)
            </label>
            <input
              type="number"
              name="target_position_mm"
              value={formData.target_position_mm}
              onChange={handleChange}
              step="0.1"
              min="0"
              max="100"
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-blue-500 focus:border-transparent"
            />
            <p className="text-xs text-gray-500 mt-1">
              Depth for Phase 1 descent (mm)
            </p>
          </div>

          {/* Descent Rate */}
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Descent Rate (mm/min)
            </label>
            <input
              type="number"
              name="descent_rate_mm_per_min"
              value={formData.descent_rate_mm_per_min}
              onChange={handleChange}
              step="0.01"
              min="0.1"
              max="10"
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-blue-500 focus:border-transparent"
            />
            <p className="text-xs text-gray-500 mt-1">
              Phase 2 descent speed (3.33 = 10mm/3min)
            </p>
          </div>

          {/* Max Duration */}
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Max Duration (seconds)
            </label>
            <input
              type="number"
              name="max_duration_s"
              value={formData.max_duration_s}
              onChange={handleChange}
              step="10"
              min="30"
              max="300"
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-blue-500 focus:border-transparent"
            />
            <p className="text-xs text-gray-500 mt-1">
              Maximum init duration before timeout (default 120s)
            </p>
          </div>

          {/* Auto-Init Interval */}
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Auto-Init Frequency
            </label>
            <select
              name="auto_init_interval"
              value={formData.auto_init_interval}
              onChange={handleChange}
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-blue-500 focus:border-transparent"
            >
              <option value={0}>Manual Only</option>
              <option value={5}>Every 5 cycles</option>
              <option value={10}>Every 10 cycles</option>
              <option value={30}>Every 30 cycles</option>
              <option value={60}>Every 60 cycles</option>
            </select>
            <p className="text-xs text-gray-500 mt-1">
              How often to automatically run init (0 = manual only)
            </p>
          </div>

          {/* Buttons */}
          <div className="flex gap-2 pt-4 border-t">
            <button
              type="submit"
              disabled={loading}
              className={`flex-1 px-4 py-2 rounded font-medium transition-colors ${
                loading
                  ? "bg-gray-300 cursor-not-allowed text-gray-500"
                  : "bg-blue-500 hover:bg-blue-600 text-white"
              }`}
            >
              {loading ? "Saving..." : "Save"}
            </button>
            <button
              type="button"
              onClick={onCancel}
              disabled={loading}
              className="flex-1 px-4 py-2 rounded font-medium border border-gray-300 hover:bg-gray-50 transition-colors"
            >
              Cancel
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default InitConfigDialog;
